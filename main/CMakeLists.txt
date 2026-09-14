idf_component_register(
    SRCS "main.c" "bsp_backlight.c" "bsp_display.c" "clock_ui.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_ledc esp_lcd_jd9855 esp_lcd_touch_ft5x06 esp_lvgl_port lvgl
)
