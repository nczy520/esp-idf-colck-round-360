idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd_jd9855 esp_lcd_touch_ft5x06 esp_lvgl_port lvgl
)
