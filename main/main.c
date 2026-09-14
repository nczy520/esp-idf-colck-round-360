#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "esp_lcd_jd9855.h"
#include "esp_lcd_touch_ft5x06.h"

#define LCD_HOST                 SPI2_HOST
#define LCD_H_RES                360
#define LCD_V_RES                360
#define LCD_BITS_PER_PIXEL       16

#define LCD_PIN_CS               GPIO_NUM_1
#define LCD_PIN_SCLK             GPIO_NUM_2
#define LCD_PIN_D0               GPIO_NUM_4
#define LCD_PIN_D1               GPIO_NUM_3
#define LCD_PIN_D2               GPIO_NUM_5
#define LCD_PIN_D3               GPIO_NUM_6
#define LCD_PIN_RST              GPIO_NUM_NC
#define LCD_PIN_BL               GPIO_NUM_0
#define LCD_BACKLIGHT_ACTIVE_LEVEL 1

#define I2C_PIN_SDA              GPIO_NUM_8
#define I2C_PIN_SCL              GPIO_NUM_7
#define I2C_PORT                 I2C_NUM_0
#define I2C_FREQ_HZ              400000
#define TOUCH_I2C_ADDRESS        0x15

static const char *TAG = "round_lcd";
static esp_lcd_panel_io_handle_t lcd_io;
static esp_lcd_panel_handle_t lcd_panel;
static esp_lcd_panel_io_handle_t touch_io;
static esp_lcd_touch_handle_t touch_handle;
static lv_display_t *display;
static lv_obj_t *touch_label;
static unsigned int touch_count;

static esp_err_t init_backlight(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << LCD_PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "backlight gpio config failed");
    return gpio_set_level(LCD_PIN_BL, !LCD_BACKLIGHT_ACTIVE_LEVEL);
}

static esp_err_t enable_backlight(void)
{
    return gpio_set_level(LCD_PIN_BL, LCD_BACKLIGHT_ACTIVE_LEVEL);
}

static esp_err_t init_lcd(void)
{
    const spi_bus_config_t bus_config = SPD2010_PANEL_BUS_QSPI_CONFIG(
        LCD_PIN_SCLK, LCD_PIN_D0, LCD_PIN_D1, LCD_PIN_D2, LCD_PIN_D3,
        LCD_H_RES * LCD_V_RES * LCD_BITS_PER_PIXEL / 8);
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO), TAG,
                        "QSPI bus init failed");

    const esp_lcd_panel_io_spi_config_t io_config = SPD2010_PANEL_IO_QSPI_CONFIG(
        LCD_PIN_CS, NULL, NULL);
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(LCD_HOST, &io_config, &lcd_io), TAG,
                        "LCD IO init failed");

    const spd2010_vendor_config_t vendor_config = {
        .flags = {.use_qspi_interface = 1},
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = LCD_BITS_PER_PIXEL,
        .vendor_config = (void *)&vendor_config,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_jd9855(lcd_io, &panel_config, &lcd_panel), TAG,
                        "JD9855 panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(lcd_panel), TAG, "LCD reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(lcd_panel), TAG, "LCD init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(lcd_panel, 0, 0), TAG, "LCD gap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(lcd_panel, true), TAG, "LCD display on failed");
    return ESP_OK;
}

static esp_err_t init_touch(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_PIN_SDA,
        .scl_io_num = I2C_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus_handle), TAG,
                        "I2C master bus init failed");

    const esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = TOUCH_I2C_ADDRESS,
        .scl_speed_hz = I2C_FREQ_HZ,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags.disable_control_phase = 1,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus_handle, &io_config, &touch_io), TAG,
                        "touch IO init failed");

    const esp_lcd_touch_config_t touch_config = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = 0, .mirror_x = 1, .mirror_y = 1},
    };
    return esp_lcd_touch_new_i2c_ft5x06(touch_io, &touch_config, &touch_handle);
}

static void touch_event_cb(lv_event_t *event)
{
    lv_indev_t *indev = lv_event_get_indev(event);
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    touch_count++;
    lv_label_set_text_fmt(touch_label, "Touch %u\nX %" PRId32 "  Y %" PRId32,
                          touch_count, point.x, point.y);
    ESP_LOGI(TAG, "touch %u: x=%d y=%d", touch_count, point.x, point.y);
}

static esp_err_t init_lvgl(void)
{
    const lvgl_port_cfg_t lvgl_config = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_config), TAG, "LVGL init failed");

    const lvgl_port_display_cfg_t display_config = {
        .io_handle = lcd_io,
        .panel_handle = lcd_panel,
        .buffer_size = LCD_H_RES * 80,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
        .flags = {.buff_dma = true, .swap_bytes = true},
    };
    display = lvgl_port_add_disp(&display_config);
    ESP_RETURN_ON_FALSE(display, ESP_FAIL, TAG, "LVGL display creation failed");

    const lvgl_port_touch_cfg_t touch_config = {
        .disp = display,
        .handle = touch_handle,
    };
    ESP_RETURN_ON_FALSE(lvgl_port_add_touch(&touch_config), ESP_FAIL, TAG,
                        "LVGL touch creation failed");

    lvgl_port_lock(0);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "360 x 360  LCD OK");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 62);

    touch_label = lv_label_create(screen);
    lv_label_set_text(touch_label, "Touch the screen");
    lv_obj_set_style_text_color(touch_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(touch_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(touch_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(touch_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(touch_label, touch_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *footer = lv_label_create(screen);
    lv_label_set_text(footer, "JD9855 / QSPI / FT5x06");
    lv_obj_set_style_text_color(footer, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -62);
    lvgl_port_unlock();
    return ESP_OK;
}

void app_main(void)
{
    ESP_ERROR_CHECK(init_backlight());
    ESP_ERROR_CHECK(init_lcd());
    ESP_ERROR_CHECK(init_touch());
    ESP_ERROR_CHECK(init_lvgl());
    ESP_ERROR_CHECK(enable_backlight());
    ESP_LOGI(TAG, "minimal LCD and touch test is running");
}
