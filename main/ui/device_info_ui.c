#include "device_info_ui.h"

#include <stdint.h>

#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_lvgl_port.h"
#include "esp_system.h"
#include "esp_flash.h"
#include "esp_idf_version.h"
#include "lvgl.h"

#include "bsp_display.h"

LV_FONT_DECLARE(lv_font_cjk_clock_18);

#define DEVICE_INFO_WINDOW_SIZE BSP_LCD_H_RES
#define DEVICE_INFO_COLOR_PANEL 0x0B1220
#define DEVICE_INFO_COLOR_BORDER 0x475569
#define DEVICE_INFO_COLOR_TEXT 0xF9FAFB
#define DEVICE_INFO_COLOR_MUTED 0x9CA3AF
#define DEVICE_INFO_COLOR_ACTION 0x1F2937

static lv_obj_t *device_info_root;

static const char *chip_model_name(esp_chip_model_t model)
{
    switch (model) {
    case CHIP_ESP32:
        return "ESP32";
    case CHIP_ESP32S2:
        return "ESP32-S2";
    case CHIP_ESP32S3:
        return "ESP32-S3";
    case CHIP_ESP32C3:
        return "ESP32-C3";
    case CHIP_ESP32C6:
        return "ESP32-C6";
    case CHIP_ESP32H2:
        return "ESP32-H2";
    default:
        return "ESP32";
    }
}

static void style_button(lv_obj_t *button)
{
    lv_obj_set_style_bg_color(button, lv_color_hex(DEVICE_INFO_COLOR_ACTION), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_set_scrollable(button, false);
}

static void close_clicked_cb(lv_event_t *event)
{
    (void)event;
    lv_obj_delete(device_info_root);
    device_info_root = NULL;
}

static void create_info_row(lv_obj_t *parent, int y, const char *title, const char *value)
{
    lv_obj_t *title_label = lv_label_create(parent);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(DEVICE_INFO_COLOR_MUTED), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_cjk_clock_18, 0);
    lv_obj_set_pos(title_label, 50, y);

    lv_obj_t *value_label = lv_label_create(parent);
    lv_label_set_text(value_label, value);
    lv_obj_set_width(value_label, 180);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(value_label, lv_color_hex(DEVICE_INFO_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(value_label, &lv_font_cjk_clock_18, 0);
    lv_obj_set_pos(value_label, 130, y);
}

void device_info_ui_show(void)
{
    if (device_info_root) {
        return;
    }

    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;
    char core_count[16];
    char feature_set[32];
    char flash_capacity[16];
    char heap_free[16];
    char screen_size[16];

    esp_chip_info(&chip_info);
    esp_flash_get_size(NULL, &flash_size);
    snprintf(core_count, sizeof(core_count), "%d 核", chip_info.cores);
    snprintf(feature_set, sizeof(feature_set), "%s%s",
             chip_info.features & CHIP_FEATURE_WIFI_BGN ? "Wi-Fi " : "",
             chip_info.features & CHIP_FEATURE_BLE ? "BLE" : "");
    snprintf(flash_capacity, sizeof(flash_capacity), "%lu MB", (unsigned long)(flash_size / (1024 * 1024)));
    snprintf(heap_free, sizeof(heap_free), "%lu KB",
             (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024));
    snprintf(screen_size, sizeof(screen_size), "%dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);

    lvgl_port_lock(0);
    device_info_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(device_info_root, DEVICE_INFO_WINDOW_SIZE, DEVICE_INFO_WINDOW_SIZE);
    lv_obj_set_pos(device_info_root, 0, 0);
    lv_obj_set_style_bg_color(device_info_root, lv_color_hex(DEVICE_INFO_COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(device_info_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(device_info_root, lv_color_hex(DEVICE_INFO_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(device_info_root, 2, 0);
    lv_obj_set_style_radius(device_info_root, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(device_info_root, 0, 0);
    lv_obj_set_scrollable(device_info_root, false);

    lv_obj_t *dial_ring = lv_obj_create(device_info_root);
    lv_obj_set_size(dial_ring, 286, 286);
    lv_obj_align(dial_ring, LV_ALIGN_CENTER, 0, 4);
    lv_obj_set_style_bg_opa(dial_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(dial_ring, lv_color_hex(DEVICE_INFO_COLOR_BORDER), 0);
    lv_obj_set_style_border_opa(dial_ring, LV_OPA_50, 0);
    lv_obj_set_style_border_width(dial_ring, 1, 0);
    lv_obj_set_style_radius(dial_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_clickable(dial_ring, false);
    lv_obj_set_scrollable(dial_ring, false);

    lv_obj_t *title = lv_label_create(device_info_root);
    lv_label_set_text(title, "设备信息");
    lv_obj_set_style_text_color(title, lv_color_hex(DEVICE_INFO_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(title, &lv_font_cjk_clock_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 62);

    create_info_row(device_info_root, 112, "芯片", chip_model_name(chip_info.model));
    create_info_row(device_info_root, 140, "核心", core_count);
    create_info_row(device_info_root, 168, "连接", feature_set);
    create_info_row(device_info_root, 196, "闪存", flash_capacity);
    create_info_row(device_info_root, 224, "内存", heap_free);
    create_info_row(device_info_root, 252, "屏幕", screen_size);
    create_info_row(device_info_root, 280, "SDK", esp_get_idf_version());

    lv_obj_t *close_button = lv_button_create(device_info_root);
    lv_obj_set_size(close_button, 42, 42);
    lv_obj_set_pos(close_button, 255, 57);
    style_button(close_button);
    lv_obj_add_event_cb(close_button, close_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_label = lv_label_create(close_button);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_label, lv_color_hex(DEVICE_INFO_COLOR_TEXT), 0);
    lv_obj_align(close_label, LV_ALIGN_CENTER, 0, 0);

    lvgl_port_unlock();
}