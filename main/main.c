#include "esp_err.h"
#include "esp_log.h"

#include "bsp_backlight.h"
#include "bsp_display.h"
#include "clock_ui.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_backlight_init());
    ESP_ERROR_CHECK(bsp_display_init());
    ESP_ERROR_CHECK(clock_ui_create());
    ESP_ERROR_CHECK(bsp_backlight_enable());
    ESP_LOGI(TAG, "LVGL clock face is running");
}
