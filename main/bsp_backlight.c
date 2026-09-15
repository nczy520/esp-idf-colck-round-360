#include "bsp_backlight.h"

#include "driver/ledc.h"
#include "esp_check.h"
#include "app_config.h"

#define LCD_BACKLIGHT_PWM_MAX_DUTY       ((1U << APP_BACKLIGHT_PWM_RESOLUTION) - 1U)

#if APP_BACKLIGHT_BRIGHTNESS_PERCENT < 0 || APP_BACKLIGHT_BRIGHTNESS_PERCENT > 100
#error "LCD_BACKLIGHT_BRIGHTNESS_PERCENT must be between 0 and 100"
#endif

static const char *TAG = "bsp_backlight";

esp_err_t bsp_backlight_init(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = APP_BACKLIGHT_PWM_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = APP_BACKLIGHT_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "backlight timer config failed");

    const ledc_channel_config_t channel_config = {
        .gpio_num = APP_BACKLIGHT_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = APP_BACKLIGHT_ACTIVE_LEVEL ? 0 : LCD_BACKLIGHT_PWM_MAX_DUTY,
        .hpoint = 0,
    };
    return ledc_channel_config(&channel_config);
}

esp_err_t bsp_backlight_enable(void)
{
    uint32_t duty = (LCD_BACKLIGHT_PWM_MAX_DUTY * APP_BACKLIGHT_BRIGHTNESS_PERCENT) / 100U;
    if (!APP_BACKLIGHT_ACTIVE_LEVEL) {
        duty = LCD_BACKLIGHT_PWM_MAX_DUTY - duty;
    }
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty), TAG,
                        "backlight duty set failed");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
