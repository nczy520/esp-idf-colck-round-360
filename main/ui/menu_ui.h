#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 在当前活跃屏幕上注册长按监听，长按后弹出圆形功能菜单。
 *
 * 依赖 `bsp_display_init()` 与 `clock_ui_create()` 已完成；菜单以叠加层形式
 * 覆盖于现有表盘之上。仅展示功能按钮，按钮点击不触发实际功能。
 */
esp_err_t menu_ui_init(void);

/**
 * @brief 显式弹出菜单（如外部代码需要主动触发）。
 */
void menu_ui_show(void);

/**
 * @brief 显式关闭菜单。
 */
void menu_ui_hide(void);

#ifdef __cplusplus
}
#endif
