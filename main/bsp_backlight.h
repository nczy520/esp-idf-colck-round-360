#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LCD 背光 PWM（定时器 + 通道，默认关闭输出）。
 */
esp_err_t bsp_backlight_init(void);

/**
 * @brief 按 `LCD_BACKLIGHT_BRIGHTNESS_PERCENT` 启用背光输出。
 */
esp_err_t bsp_backlight_enable(void);

#ifdef __cplusplus
}
#endif
