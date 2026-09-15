#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  WEATHER_SUNNY,
  WEATHER_CLOUDY,
  WEATHER_OVERCAST,
  WEATHER_RAINY,
  WEATHER_THUNDERSTORM,
  WEATHER_SNOWY,
  WEATHER_FOGGY,
  WEATHER_COUNT,
} weather_type_t;

extern const lv_image_dsc_t *weather_icons[WEATHER_COUNT];

#ifdef __cplusplus
}
#endif
