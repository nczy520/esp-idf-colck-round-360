#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_LCD_H_RES        APP_LCD_H_RES
#define BSP_LCD_V_RES        APP_LCD_V_RES
#define BSP_LCD_BITS_PER_PX  APP_LCD_BITS_PER_PX
#define BSP_LCD_ROTATION_DEG APP_LCD_ROTATION_DEG

#if BSP_LCD_ROTATION_DEG == 0
#define BSP_LCD_ROTATION     LV_DISPLAY_ROTATION_0
#elif BSP_LCD_ROTATION_DEG == 90
#define BSP_LCD_ROTATION     LV_DISPLAY_ROTATION_90
#elif BSP_LCD_ROTATION_DEG == 180
#define BSP_LCD_ROTATION     LV_DISPLAY_ROTATION_180
#elif BSP_LCD_ROTATION_DEG == 270
#define BSP_LCD_ROTATION     LV_DISPLAY_ROTATION_270
#else
#error "BSP_LCD_ROTATION_DEG must be 0, 90, 180, or 270"
#endif

/**
 * @brief 初始化 SPI LCD、I2C 触摸以及 LVGL 显示/触摸端口。
 *
 * 调用成功后即可在 `lv_screen_active()` 上构建 UI。
 */
esp_err_t bsp_display_init(void);

#ifdef __cplusplus
}
#endif
