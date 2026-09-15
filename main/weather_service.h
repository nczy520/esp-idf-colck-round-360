#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t weather_service_start(void);

/**
 * @brief 从 NVS 立即恢复天气显示内容。
 */
bool weather_service_restore_cache(void);

#ifdef __cplusplus
}
#endif
