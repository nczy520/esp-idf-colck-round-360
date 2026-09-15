#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "lvgl.h"

/* LCD and touch hardware */
#define APP_LCD_HOST                    SPI2_HOST
#define APP_LCD_PIN_CS                  GPIO_NUM_1
#define APP_LCD_PIN_SCLK                GPIO_NUM_2
#define APP_LCD_PIN_D0                  GPIO_NUM_4
#define APP_LCD_PIN_D1                  GPIO_NUM_3
#define APP_LCD_PIN_D2                  GPIO_NUM_5
#define APP_LCD_PIN_D3                  GPIO_NUM_6
#define APP_LCD_PIN_RST                 GPIO_NUM_NC
#define APP_LCD_H_RES                   360
#define APP_LCD_V_RES                   360
#define APP_LCD_BITS_PER_PX             16
#define APP_LCD_ROTATION_DEG            270
#define APP_TOUCH_PIN_SDA               GPIO_NUM_8
#define APP_TOUCH_PIN_SCL               GPIO_NUM_7
#define APP_TOUCH_PORT                  I2C_NUM_0
#define APP_TOUCH_FREQ_HZ               400000
#define APP_TOUCH_ADDRESS               0x15

/* Backlight */
#define APP_BACKLIGHT_PIN               GPIO_NUM_0
#define APP_BACKLIGHT_ACTIVE_LEVEL      1
#define APP_BACKLIGHT_BRIGHTNESS_PERCENT 33
#define APP_BACKLIGHT_PWM_FREQUENCY_HZ  10000
#define APP_BACKLIGHT_PWM_RESOLUTION    LEDC_TIMER_8_BIT

/* Clock UI */
#define APP_CLOCK_COLOR_BG              0xFFFFFF
#define APP_CLOCK_COLOR_SURFACE         0xF9FAFB
#define APP_CLOCK_COLOR_INK             0x1F2937
#define APP_CLOCK_COLOR_INK_2           0x6B7280
#define APP_CLOCK_COLOR_INK_3           0x9CA3AF
#define APP_CLOCK_COLOR_LINE            0xE5E7EB
#define APP_CLOCK_COLOR_BRAND           0x0D9488
#define APP_CLOCK_PI                    3.14159265358979323846f
#define APP_CLOCK_HOUR_HAND_LENGTH      80
#define APP_CLOCK_MINUTE_HAND_LENGTH    109
#define APP_CLOCK_SECOND_HAND_LENGTH    124
#define APP_CLOCK_SECOND_TAIL_LENGTH    15
#define APP_CLOCK_TICK_MAJOR_OUTER      175
#define APP_CLOCK_TICK_MAJOR_INNER      158
#define APP_CLOCK_TICK_MINOR_OUTER      171
#define APP_CLOCK_TICK_MINOR_INNER      162
#define APP_CLOCK_RING_OUTER_RADIUS     180
#define APP_CLOCK_NUMBER_RADIUS         144
#define APP_CLOCK_WEATHER_TEXT          "天气加载中"
#define APP_CLOCK_WEATHER_ROW_WIDTH     290
#define APP_CLOCK_WEATHER_ROW_HEIGHT    64
#define APP_CLOCK_WEATHER_ROW_Y         78
#define APP_CLOCK_WEATHER_TEXT_WIDTH    258
#define APP_CLOCK_PERIOD_LABEL_WIDTH    48
#define APP_CLOCK_PERIOD_LABEL_X        245
#define APP_CLOCK_PERIOD_LABEL_Y        170
#define APP_CLOCK_DATE_LABEL_Y          222
#define APP_CLOCK_LUNAR_LABEL_Y         250

/* Weather service */
#define APP_WEATHER_CACHE_NAMESPACE     "weather"
#define APP_WEATHER_CACHE_KEY           "current"
#define APP_WEATHER_URL_FORMAT          "https://w.mdeve.com/%s,n0.ics"
#define APP_WEATHER_UPDATE_INTERVAL_SEC (30 * 60)
#define APP_WEATHER_RETRY_INTERVAL_SEC  60
#define APP_WEATHER_RESPONSE_BUFFER_SIZE 12288
#define APP_WEATHER_TEXT_SIZE            128
#define APP_WEATHER_DATE_SIZE            9
#define APP_WEATHER_LUNAR_SIZE           64
#define APP_WEATHER_GANZHI_SIZE          64
#define APP_WEATHER_TERM_SIZE            32
#define APP_WEATHER_TASK_STACK_BYTES    12288
