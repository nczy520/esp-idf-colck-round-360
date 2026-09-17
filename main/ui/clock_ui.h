#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 在当前活跃屏幕上创建圆形时钟表盘并启动每秒刷新定时器。
 *
 * 依赖 `bsp_display_init()` 已成功执行；调用前需保证 LVGL 端口已就绪。
 */
esp_err_t clock_ui_create(void);

/**
 * @brief 更新天气标签文本。
 */
void clock_ui_set_weather(const char *text);

/**
 * @brief 暂停表盘动画刷新。
 */
void clock_ui_pause(void);

/**
 * @brief 恢复表盘动画刷新。
 */
void clock_ui_resume(void);

#ifdef __cplusplus
}
#endif
