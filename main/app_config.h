#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"

/* LCD and touch hardware. */
#define APP_LCD_HOST                    SPI2_HOST       /* QSPI host. */
#define APP_LCD_PIN_CS                  GPIO_NUM_1     /* LCD chip select. */
#define APP_LCD_PIN_SCLK                GPIO_NUM_2     /* QSPI clock. */
#define APP_LCD_PIN_D0                  GPIO_NUM_4     /* QSPI data 0. */
#define APP_LCD_PIN_D1                  GPIO_NUM_3     /* QSPI data 1. */
#define APP_LCD_PIN_D2                  GPIO_NUM_5     /* QSPI data 2. */
#define APP_LCD_PIN_D3                  GPIO_NUM_6     /* QSPI data 3. */
#define APP_LCD_PIN_RST                 GPIO_NUM_NC    /* LCD reset is not connected. */
#define APP_LCD_H_RES                   360            /* Horizontal resolution, pixels. */
#define APP_LCD_V_RES                   360            /* Vertical resolution, pixels. */
#define APP_LCD_BITS_PER_PX             16             /* RGB565 color depth. */
#define APP_LCD_ROTATION_DEG            270            /* Display rotation, degrees. */
#define APP_TOUCH_PIN_SDA               GPIO_NUM_8     /* Touch I2C data. */
#define APP_TOUCH_PIN_SCL               GPIO_NUM_7     /* Touch I2C clock. */
#define APP_TOUCH_PORT                  I2C_NUM_0      /* Touch I2C controller. */
#define APP_TOUCH_FREQ_HZ               400000         /* Touch I2C frequency, Hz. */
#define APP_TOUCH_ADDRESS               0x15           /* Touch device I2C address. */

/* Backlight. */
#define APP_BACKLIGHT_PIN               GPIO_NUM_0     /* Backlight control pin. */
#define APP_BACKLIGHT_ACTIVE_LEVEL      1              /* Active-high output. */
#define APP_BACKLIGHT_BRIGHTNESS_PERCENT 33            /* Brightness, percent. */
#define APP_BACKLIGHT_PWM_FREQUENCY_HZ  10000          /* PWM frequency, Hz. */
#define APP_BACKLIGHT_PWM_RESOLUTION    LEDC_TIMER_8_BIT /* PWM resolution. */

/* Clock UI. Colors use 24-bit RGB hexadecimal values. */
#define APP_CLOCK_COLOR_BG              0xFFFFFF       /* Screen background. */
#define APP_CLOCK_COLOR_SURFACE         0xF9FAFB       /* Clock face fill. */
#define APP_CLOCK_COLOR_INK             0x1F2937       /* Primary hand/text color. */
#define APP_CLOCK_COLOR_INK_2           0x6B7280       /* Secondary text/tick color. */
#define APP_CLOCK_COLOR_INK_3           0x9CA3AF       /* Muted text/tick color. */
#define APP_CLOCK_COLOR_LINE            0xE5E7EB       /* Clock ring color. */
#define APP_CLOCK_COLOR_BRAND           0x0D9488       /* Second hand/accent color. */
#define APP_CLOCK_PI                    3.14159265358979323846f /* Circle constant. */
#define APP_CLOCK_HOUR_HAND_LENGTH      80             /* Hour hand length, pixels. */
#define APP_CLOCK_MINUTE_HAND_LENGTH    109            /* Minute hand length, pixels. */
#define APP_CLOCK_SECOND_HAND_LENGTH    124            /* Second hand length, pixels. */
#define APP_CLOCK_SECOND_TAIL_LENGTH    15             /* Second hand tail, pixels. */
#define APP_CLOCK_TICK_MAJOR_OUTER      175            /* Major tick outer radius. */
#define APP_CLOCK_TICK_MAJOR_INNER      158            /* Major tick inner radius. */
#define APP_CLOCK_TICK_MINOR_OUTER      171            /* Minor tick outer radius. */
#define APP_CLOCK_TICK_MINOR_INNER      162            /* Minor tick inner radius. */
#define APP_CLOCK_RING_OUTER_RADIUS     180            /* Clock ring radius. */
#define APP_CLOCK_NUMBER_RADIUS         144            /* Hour number radius. */
#define APP_CLOCK_WEATHER_TEXT          "天气加载中"   /* Startup weather text. */
#define APP_CLOCK_WEATHER_ROW_WIDTH     290            /* Weather container width. */
#define APP_CLOCK_WEATHER_ROW_HEIGHT    64             /* Weather container height. */
#define APP_CLOCK_WEATHER_ROW_Y         78             /* Weather container Y position. */
#define APP_CLOCK_WEATHER_TEXT_WIDTH    258            /* Weather text width. */
#define APP_CLOCK_PERIOD_LABEL_WIDTH    48             /* AM/PM label width. */
#define APP_CLOCK_PERIOD_LABEL_X        255            /* AM/PM label X position. */
#define APP_CLOCK_PERIOD_LABEL_Y        170            /* AM/PM label Y position. */
#define APP_CLOCK_DATE_LABEL_Y          222            /* Date label Y position. */
#define APP_CLOCK_LUNAR_LABEL_Y         250            /* Lunar label Y position. */

/* Weather service. */
#define APP_WEATHER_CACHE_NAMESPACE     "weather"      /* NVS namespace. */
#define APP_WEATHER_CACHE_KEY           "current"      /* NVS cache key. */
#define APP_WEATHER_URL_FORMAT          "https://w.mdeve.com/%s,n0.ics" /* %s is city code. */
#define APP_WEATHER_UPDATE_INTERVAL_SEC (30 * 60)       /* Network update interval. */
#define APP_WEATHER_RETRY_INTERVAL_SEC  60              /* Offline retry interval, seconds. */
#define APP_WEATHER_RESPONSE_BUFFER_SIZE 12288          /* ICS response buffer, bytes. */
#define APP_WEATHER_TEXT_SIZE            128             /* Weather text buffer, bytes. */
#define APP_WEATHER_DATE_SIZE            9               /* YYYYMMDD plus terminator. */
#define APP_WEATHER_LUNAR_SIZE           64              /* Lunar text buffer, bytes. */
#define APP_WEATHER_GANZHI_SIZE          64              /* Ganzhi text buffer, bytes. */
#define APP_WEATHER_TERM_SIZE            32              /* Term/festival buffer, bytes. */
#define APP_WEATHER_TASK_STACK_BYTES    12288           /* Weather task stack, bytes. */
