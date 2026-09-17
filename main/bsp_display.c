#include "bsp_display.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "esp_lcd_jd9855.h"
#include "esp_lcd_touch_ft5x06.h"
#include "app_config.h"

static const char *TAG = "bsp_display";
static esp_lcd_panel_io_handle_t lcd_io;
static esp_lcd_panel_handle_t lcd_panel;
static esp_lcd_panel_io_handle_t touch_io;
static esp_lcd_touch_handle_t touch_handle;
static lv_display_t *display;

static esp_err_t init_lcd(void)
{
    const spi_bus_config_t bus_config = SPD2010_PANEL_BUS_QSPI_CONFIG(
        APP_LCD_PIN_SCLK, APP_LCD_PIN_D0, APP_LCD_PIN_D1, APP_LCD_PIN_D2, APP_LCD_PIN_D3,
        BSP_LCD_H_RES * BSP_LCD_V_RES * BSP_LCD_BITS_PER_PX / 8);
    ESP_RETURN_ON_ERROR(spi_bus_initialize(APP_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO), TAG,
                        "QSPI bus init failed");

    const esp_lcd_panel_io_spi_config_t io_config = SPD2010_PANEL_IO_QSPI_CONFIG(
        APP_LCD_PIN_CS, NULL, NULL);
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(APP_LCD_HOST, &io_config, &lcd_io), TAG,
                        "LCD IO init failed");

    const spd2010_vendor_config_t vendor_config = {
        .flags = {.use_qspi_interface = 1},
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = APP_LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = BSP_LCD_BITS_PER_PX,
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
        .i2c_port = APP_TOUCH_PORT,
        .sda_io_num = APP_TOUCH_PIN_SDA,
        .scl_io_num = APP_TOUCH_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus_handle), TAG,
                        "I2C master bus init failed");

    const esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = APP_TOUCH_ADDRESS,
        .scl_speed_hz = APP_TOUCH_FREQ_HZ,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags.disable_control_phase = 1,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus_handle, &io_config, &touch_io), TAG,
                        "touch IO init failed");

    const esp_lcd_touch_config_t touch_config = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
    };
    return esp_lcd_touch_new_i2c_ft5x06(touch_io, &touch_config, &touch_handle);
}

static esp_err_t init_lvgl(void)
{
    const lvgl_port_cfg_t lvgl_config = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_config), TAG, "LVGL init failed");

    const lvgl_port_display_cfg_t display_config = {
        .io_handle = lcd_io,
        .panel_handle = lcd_panel,
        .buffer_size = BSP_LCD_H_RES * 20,
        .double_buffer = true,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
        .flags = {.buff_dma = true, .buff_spiram = false, .swap_bytes = true, .sw_rotate = true},
    };
    display = lvgl_port_add_disp(&display_config);
    ESP_RETURN_ON_FALSE(display, ESP_FAIL, TAG, "LVGL display creation failed");
    lv_display_set_rotation(display, BSP_LCD_ROTATION);

    const lvgl_port_touch_cfg_t touch_config = {
        .disp = display,
        .handle = touch_handle,
    };
    ESP_RETURN_ON_FALSE(lvgl_port_add_touch(&touch_config), ESP_FAIL, TAG,
                        "LVGL touch creation failed");
    return ESP_OK;
}

esp_err_t bsp_display_init(void)
{
    ESP_RETURN_ON_ERROR(init_lcd(), TAG, "LCD init failed");
    ESP_RETURN_ON_ERROR(init_touch(), TAG, "touch init failed");
    ESP_RETURN_ON_ERROR(init_lvgl(), TAG, "LVGL init failed");
    return ESP_OK;
}
