#include "weather_service.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include "ble_srv.h"
#include "clock_ui.h"
#include "app_config.h"

static const char *TAG = "weather_service";

typedef struct {
    char date[APP_WEATHER_DATE_SIZE];
    char weather[APP_WEATHER_TEXT_SIZE];
    char lunar[APP_WEATHER_LUNAR_SIZE];
    char ganzhi[APP_WEATHER_GANZHI_SIZE];
    char solar_term[APP_WEATHER_TERM_SIZE];
    char festival[APP_WEATHER_TERM_SIZE];
    int64_t updated_at;
} weather_cache_t;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} http_response_t;

static weather_cache_t cached_weather;
static bool cache_loaded;

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    http_response_t *response = event->user_data;
    if (event->event_id == HTTP_EVENT_ERROR) {
        ESP_LOGD(TAG, "HTTP event: error");
    } else if (event->event_id == HTTP_EVENT_ON_CONNECTED) {
        ESP_LOGD(TAG, "HTTP event: connected");
    } else if (event->event_id == HTTP_EVENT_HEADERS_SENT) {
        ESP_LOGD(TAG, "HTTP event: headers sent");
    } else if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key && event->header_value) {
        ESP_LOGD(TAG, "HTTP header: %s=%s", event->header_key, event->header_value);
    } else if (event->event_id == HTTP_EVENT_ON_FINISH) {
        ESP_LOGD(TAG, "HTTP event: finished");
    } else if (event->event_id == HTTP_EVENT_DISCONNECTED) {
        ESP_LOGD(TAG, "HTTP event: disconnected");
    }

    if (event->event_id != HTTP_EVENT_ON_DATA || !response || !event->data_len) {
        return ESP_OK;
    }
    if (response->length + event->data_len >= response->capacity) {
        ESP_LOGE(TAG, "HTTP response too large: current=%u incoming=%d capacity=%u",
                 (unsigned)response->length, event->data_len, (unsigned)response->capacity);
        return ESP_ERR_NO_MEM;
    }
    memcpy(response->data + response->length, event->data, event->data_len);
    response->length += event->data_len;
    response->data[response->length] = '\0';
    ESP_LOGD(TAG, "HTTP data chunk: %d bytes, total=%u",
             event->data_len, (unsigned)response->length);
    return ESP_OK;
}

static void copy_field(char *destination, size_t destination_size,
                       const char *start, const char *end)
{
    if (!start || !destination || destination_size == 0) {
        return;
    }
    size_t length = end && end > start ? (size_t)(end - start) : strlen(start);
    if (length >= destination_size) {
        length = destination_size - 1;
    }
    memcpy(destination, start, length);
    destination[length] = '\0';
}

static void extract_description_field(char *destination, size_t destination_size,
                                      const char *description, const char *field)
{
    const char *start = strstr(description, field);
    if (!start) {
        destination[0] = '\0';
        return;
    }
    start += strlen(field);
    const char *end = strstr(start, "\\n");
    copy_field(destination, destination_size, start, end);
}

static void extract_festival(char *destination, size_t destination_size, const char *description)
{
    const char *start = strstr(description, "\\n----------------------------------\\n");
    if (!start) {
        destination[0] = '\0';
        return;
    }
    start += strlen("\\n----------------------------------\\n");
    const char *end = strstr(start, "\\n");
    if (!end) {
        destination[0] = '\0';
        return;
    }
    const char *colon = strstr(start, "：");
    if (!colon || colon >= end) {
        copy_field(destination, destination_size, start, end);
        return;
    }
    copy_field(destination, destination_size, start, colon);
}

static bool parse_ical(char *ical, const char *date, weather_cache_t *result)
{
    size_t ical_length = strlen(ical);
    ESP_LOGD(TAG, "Parsing ICS: date=%s, length=%u", date, (unsigned)ical_length);

    bool in_event = false;
    bool weather_event = false;
    char event_date[APP_WEATHER_DATE_SIZE] = {0};
    char summary[APP_WEATHER_TEXT_SIZE] = {0};
    char description[2048] = {0};
    bool found_weather = false;
    bool found_lunar = false;
    int event_count = 0;
    int matching_event_count = 0;

    char *saveptr = NULL;
        for (char *line = strtok_r(ical, "\r\n", &saveptr); line;
            line = strtok_r(NULL, "\r\n", &saveptr)) {
        if (strcmp(line, "BEGIN:VEVENT") == 0) {
            event_count++;
            in_event = true;
            weather_event = false;
            event_date[0] = '\0';
            summary[0] = '\0';
            description[0] = '\0';
            continue;
        }
        if (!in_event) {
            continue;
        }
        if (strcmp(line, "END:VEVENT") == 0) {
            if (strcmp(event_date, date) == 0) {
                matching_event_count++;
                if (weather_event && summary[0]) {
                    const char *weather_start = strchr(summary, ' ');
                    copy_field(result->weather, sizeof(result->weather),
                               weather_start ? weather_start + 1 : summary, NULL);
                    found_weather = true;
                    ESP_LOGD(TAG, "Weather event: date=%s summary=%s", event_date, summary);
                } else if (description[0]) {
                    extract_description_field(result->lunar, sizeof(result->lunar), description, "农历全称：");
                    extract_description_field(result->solar_term, sizeof(result->solar_term), description, "节气：");
                    extract_festival(result->festival, sizeof(result->festival), description);
                    found_lunar = result->lunar[0] != '\0';
                    const char *ganzhi = strstr(description, "农历全称：");
                    if (ganzhi) {
                        ganzhi += strlen("农历全称：");
                        const char *end = strstr(ganzhi, "\\n");
                        copy_field(result->ganzhi, sizeof(result->ganzhi), ganzhi, end);
                    }
                    ESP_LOGD(TAG, "Calendar event: lunar=%s ganzhi=%s term=%s festival=%s",
                             result->lunar, result->ganzhi, result->solar_term, result->festival);
                }
            }
            in_event = false;
            continue;
        }
        if (strncmp(line, "DTSTART", 7) == 0) {
            const char *value = strchr(line, ':');
            if (value) {
                copy_field(event_date, sizeof(event_date), value + 1, NULL);
            }
        } else if (strncmp(line, "CATEGORIES:", 11) == 0) {
            weather_event = strcmp(line + 11, "WEATHER") == 0;
        } else if (strncmp(line, "SUMMARY:", 8) == 0) {
            copy_field(summary, sizeof(summary), line + 8, NULL);
        } else if (strncmp(line, "DESCRIPTION:", 12) == 0) {
            copy_field(description, sizeof(description), line + 12, NULL);
        }
    }
    snprintf(result->date, sizeof(result->date), "%s", date);
    ESP_LOGD(TAG, "ICS parse result: events=%d matching=%d weather=%s lunar=%s",
             event_count, matching_event_count,
             found_weather ? "yes" : "no", found_lunar ? "yes" : "no");
    return found_weather || found_lunar;
}

static bool load_cache(void)
{
    if (cache_loaded) {
        return true;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open(APP_WEATHER_CACHE_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No weather cache: nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }
    size_t size = sizeof(cached_weather);
    err = nvs_get_blob(handle, APP_WEATHER_CACHE_KEY, &cached_weather, &size);
    nvs_close(handle);
    if (err != ESP_OK || size != sizeof(cached_weather)) {
        ESP_LOGW(TAG, "Weather cache invalid: read=%s size=%u expected=%u",
                 esp_err_to_name(err), (unsigned)size, (unsigned)sizeof(cached_weather));
        return false;
    }
    clock_ui_set_weather(cached_weather.weather);
    cache_loaded = true;
    ESP_LOGI(TAG, "Weather cache loaded: date=%s weather=%s updated_at=%lld",
             cached_weather.date, cached_weather.weather, (long long)cached_weather.updated_at);
    return true;
}

bool weather_service_restore_cache(void)
{
    return load_cache();
}

static void save_cache(const weather_cache_t *weather)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(APP_WEATHER_CACHE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Weather cache open failed: %s", esp_err_to_name(err));
        return;
    }
    err = nvs_set_blob(handle, APP_WEATHER_CACHE_KEY, weather, sizeof(*weather));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Weather cache save failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGD(TAG, "Weather cache saved: date=%s", weather->date);
    }
}

static bool fetch_weather(void)
{
    time_t now = time(NULL);
    struct tm local_time;
    localtime_r(&now, &local_time);

    char date[APP_WEATHER_DATE_SIZE];
    strftime(date, sizeof(date), "%Y%m%d", &local_time);

    char url[96];
    snprintf(url, sizeof(url), APP_WEATHER_URL_FORMAT, CONFIG_WEATHER_CITY_CODE);
    ESP_LOGI(TAG, "Weather request: city=%s date=%s url=%s",
             CONFIG_WEATHER_CITY_CODE, date, url);
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "TLS memory before request: internal=%uB spiram=%uB",
             (unsigned)internal_free, (unsigned)spiram_free);
    char *response_buffer = heap_caps_malloc(APP_WEATHER_RESPONSE_BUFFER_SIZE,
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!response_buffer) {
        response_buffer = heap_caps_malloc(APP_WEATHER_RESPONSE_BUFFER_SIZE,
                                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!response_buffer) {
        ESP_LOGE(TAG, "weather response buffer allocation failed");
        return false;
    }
    http_response_t response = {
        .data = response_buffer,
        .capacity = APP_WEATHER_RESPONSE_BUFFER_SIZE,
    };
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &response,
        .timeout_ms = 15000,
        .disable_auto_redirect = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        heap_caps_free(response_buffer);
        ESP_LOGE(TAG, "HTTP client allocation failed");
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    ESP_LOGI(TAG, "Weather response: err=%s status=%d bytes=%u",
             esp_err_to_name(err), status_code, (unsigned)response.length);
    if (err != ESP_OK || status_code < 200 || status_code >= 300) {
        ESP_LOGW(TAG, "weather request failed: %s, status=%d", esp_err_to_name(err), status_code);
        heap_caps_free(response_buffer);
        return false;
    }

    weather_cache_t parsed = {0};
    bool parsed_ok = parse_ical(response_buffer, date, &parsed);
    heap_caps_free(response_buffer);
    if (!parsed_ok || !parsed.weather[0]) {
        ESP_LOGW(TAG, "weather response did not contain today's event");
        return false;
    }
    parsed.updated_at = now;
    cached_weather = parsed;
    save_cache(&cached_weather);
    clock_ui_set_weather(cached_weather.weather);
    ESP_LOGI(TAG, "weather updated for %s", date);
    return true;
}

static void weather_task(void *arg)
{
    (void)arg;
    bool cache_available = load_cache();
    ESP_LOGI(TAG, "Weather service started: cache=%s update_interval=%us",
             cache_available ? "available" : "unavailable",
             APP_WEATHER_UPDATE_INTERVAL_SEC);
    int64_t last_success = 0;
    bool last_wifi_connected = false;
    while (true) {
        int64_t now = esp_timer_get_time() / 1000000;
        bool wifi_connected = ble_srv_wifi_is_connected();
        if (wifi_connected != last_wifi_connected) {
            ESP_LOGI(TAG, "Wi-Fi state: %s", wifi_connected ? "connected" : "disconnected");
            last_wifi_connected = wifi_connected;
        }
        bool due = last_success == 0 || now - last_success >= APP_WEATHER_UPDATE_INTERVAL_SEC;
        if (due) {
            ESP_LOGD(TAG, "Weather update due: wifi=%s elapsed=%llds",
                     wifi_connected ? "yes" : "no",
                     last_success == 0 ? 0LL : (long long)(now - last_success));
        }
        if (due && wifi_connected && fetch_weather()) {
            last_success = now;
            ESP_LOGD(TAG, "Weather task stack remaining: %uB",
                     (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
        } else if (due && !wifi_connected) {
            ESP_LOGD(TAG, "Weather update skipped: Wi-Fi unavailable");
        }
        vTaskDelay(pdMS_TO_TICKS((due && !wifi_connected) ?
                     APP_WEATHER_RETRY_INTERVAL_SEC * 1000 : 10000));
    }
}

esp_err_t weather_service_start(void)
{
    if (xTaskCreateWithCaps(weather_task, "weather_task", APP_WEATHER_TASK_STACK_BYTES, NULL, 4, NULL,
                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        ESP_LOGE(TAG, "weather task creation failed: PSRAM stack unavailable");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
