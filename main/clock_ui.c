#include "clock_ui.h"

#include <math.h>
#include <string.h>
#include <time.h>

#include "esp_check.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "bsp_display.h"
#include "calendar_lunar.h"
#include "weather_icons.h"
#include "app_config.h"

/* 定制 CJK 字体：18px，含二十四节气 / 天干地支 / 农历日期 / 天气 */
LV_FONT_DECLARE(lv_font_cjk_clock_18);

/* Clock design tokens — matching LVGL 时钟表盘 design */
#define CLOCK_FACE_SIZE          APP_LCD_H_RES
#define CLOCK_CENTER             (CLOCK_FACE_SIZE / 2)
#define CJK_FONT                 (&lv_font_cjk_clock_18)

static lv_obj_t *date_label;
static lv_obj_t *lunar_label;
static lv_obj_t *weather_label;
static lv_obj_t *weather_icon;
static lv_obj_t *time_period_label;
static lv_obj_t *hour_hand;
static lv_obj_t *minute_hand;
static lv_obj_t *second_hand;
static lv_point_precise_t hour_points[2];
static lv_point_precise_t minute_points[2];
static lv_point_precise_t second_points[2];
static lv_point_precise_t minute_tick_points[60][2];

static weather_type_t weather_type_from_text(const char *text)
{
    if (strstr(text, "雷") || strstr(text, "电") || strstr(text, "冰雹")) {
        return WEATHER_THUNDERSTORM;
    }
    if (strstr(text, "雪") || strstr(text, "冻雨")) {
        return WEATHER_SNOWY;
    }
    if (strstr(text, "雨")) {
        return WEATHER_RAINY;
    }
    if (strstr(text, "雾") || strstr(text, "霾") || strstr(text, "沙尘")) {
        return WEATHER_FOGGY;
    }
    if (strstr(text, "阴")) {
        return WEATHER_OVERCAST;
    }
    if (strstr(text, "云")) {
        return WEATHER_CLOUDY;
    }
    return WEATHER_SUNNY;
}

void clock_ui_set_weather(const char *text)
{
    if (!weather_label || !weather_icon || !text) {
        return;
    }
    lvgl_port_lock(0);
    lv_label_set_text(weather_label, text);
    lv_image_set_src(weather_icon, weather_icons[weather_type_from_text(text)]);
    lvgl_port_unlock();
}

static lv_obj_t *create_clock_hand(lv_obj_t *parent, lv_color_t color, int width)
{
    lv_obj_t *hand = lv_line_create(parent);
    lv_obj_set_style_line_color(hand, color, 0);
    lv_obj_set_style_line_width(hand, width, 0);
    lv_obj_set_style_line_rounded(hand, true, 0);
    return hand;
}

static void update_clock_hand(lv_obj_t *hand, lv_point_precise_t points[2],
                              float angle, int length, int tail_length)
{
    points[0].x = CLOCK_CENTER - (lv_coord_t)lroundf(cosf(angle) * tail_length);
    points[0].y = CLOCK_CENTER - (lv_coord_t)lroundf(sinf(angle) * tail_length);
    points[1].x = CLOCK_CENTER + (lv_coord_t)lroundf(cosf(angle) * length);
    points[1].y = CLOCK_CENTER + (lv_coord_t)lroundf(sinf(angle) * length);
    lv_line_set_points(hand, points, 2);
}

static void create_minute_ticks(lv_obj_t *parent)
{
    /* 刻度分三档：
     * - major: 12/3/6/9 (i % 15 == 0)，最长最粗，ink-2
     * - medium: 1/2/4/5/7/8/10/11 (其余小时整点)，中等粗细，ink-2
     * - minor: 分钟刻度，最细，ink-3 */
    for (int i = 0; i < 60; i++) {
        float angle = (float)i * 2.0f * APP_CLOCK_PI / 60.0f - APP_CLOCK_PI / 2.0f;
        bool major = (i % 15) == 0;
        bool medium = !major && (i % 5) == 0;
        int outer_radius = major ? APP_CLOCK_TICK_MAJOR_OUTER : APP_CLOCK_TICK_MINOR_OUTER;
        int inner_radius = major ? APP_CLOCK_TICK_MAJOR_INNER : APP_CLOCK_TICK_MINOR_INNER;
        minute_tick_points[i][0].x = CLOCK_CENTER + (lv_coord_t)lroundf(cosf(angle) * inner_radius);
        minute_tick_points[i][0].y = CLOCK_CENTER + (lv_coord_t)lroundf(sinf(angle) * inner_radius);
        minute_tick_points[i][1].x = CLOCK_CENTER + (lv_coord_t)lroundf(cosf(angle) * outer_radius);
        minute_tick_points[i][1].y = CLOCK_CENTER + (lv_coord_t)lroundf(sinf(angle) * outer_radius);

        lv_obj_t *tick = lv_line_create(parent);
            lv_obj_set_style_line_color(tick,
                lv_color_hex((major || medium) ? APP_CLOCK_COLOR_INK_2 : APP_CLOCK_COLOR_INK_3), 0);
        lv_obj_set_style_line_width(tick, (major || medium) ? 3 : 2, 0);
        lv_obj_set_style_line_rounded(tick, true, 0);
        lv_line_set_points(tick, minute_tick_points[i], 2);
    }
}

static void clock_update_cb(lv_timer_t *timer)
{
    (void)timer;
    time_t now = time(NULL);
    struct tm current_time;
    localtime_r(&now, &current_time);

    static const char *weekday_names[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    static calendar_info_t calendar;
    static int cached_year = -1;
    static int cached_month = -1;
    static int cached_day = -1;
    static int cached_period = -1;

    int current_period = current_time.tm_hour < 12 ? 0 : 1;
    if (current_period != cached_period) {
        cached_period = current_period;
        lv_label_set_text(time_period_label, current_period == 0 ? "上午" : "下午");
    }

    if (current_time.tm_year != cached_year ||
        current_time.tm_mon != cached_month ||
        current_time.tm_mday != cached_day) {
        cached_year = current_time.tm_year;
        cached_month = current_time.tm_mon;
        cached_day = current_time.tm_mday;

        bool calendar_valid = calendar_get_info(&current_time, &calendar);
        lv_label_set_text_fmt(date_label, "%d年%d月%d日 %s",
                              current_time.tm_year + 1900,
                              current_time.tm_mon + 1,
                              current_time.tm_mday,
                              weekday_names[current_time.tm_wday]);
        if (calendar_valid) {
            lv_label_set_text_fmt(lunar_label, "农历%s%s%s %s%s",
                                  calendar.lunar.leap_month ? "闰" : "",
                                  calendar.lunar_month_name, calendar.lunar_day_name,
                                  calendar.festival, calendar.solar_term);
        }
    }

    float second_angle = (float)current_time.tm_sec * 2.0f * APP_CLOCK_PI / 60.0f - APP_CLOCK_PI / 2.0f;
    float minute_angle = ((float)current_time.tm_min + (float)current_time.tm_sec / 60.0f)
                         * 2.0f * APP_CLOCK_PI / 60.0f - APP_CLOCK_PI / 2.0f;
    float hour_angle = ((float)(current_time.tm_hour % 12) + (float)current_time.tm_min / 60.0f)
                       * 2.0f * APP_CLOCK_PI / 12.0f - APP_CLOCK_PI / 2.0f;

    update_clock_hand(hour_hand, hour_points, hour_angle, APP_CLOCK_HOUR_HAND_LENGTH, 0);
    update_clock_hand(minute_hand, minute_points, minute_angle, APP_CLOCK_MINUTE_HAND_LENGTH, 0);
    update_clock_hand(second_hand, second_points, second_angle,
                      APP_CLOCK_SECOND_HAND_LENGTH, APP_CLOCK_SECOND_TAIL_LENGTH);
}

esp_err_t clock_ui_create(void)
{
    lvgl_port_lock(0);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(APP_CLOCK_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    /* 外缘装饰环：r=180，线色描边 */
    lv_obj_t *outer_ring = lv_obj_create(screen);
    lv_obj_set_size(outer_ring, APP_CLOCK_RING_OUTER_RADIUS * 2, APP_CLOCK_RING_OUTER_RADIUS * 2);
    lv_obj_align(outer_ring, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(outer_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(outer_ring, lv_color_hex(APP_CLOCK_COLOR_LINE), 0);
    lv_obj_set_style_border_width(outer_ring, 1, 0);
    lv_obj_set_style_radius(outer_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(outer_ring, 0, 0);

    /* 内侧浅色填充：surface 35% 不透明度 */
    lv_obj_t *inner_fill = lv_obj_create(screen);
    lv_obj_set_size(inner_fill, (APP_CLOCK_RING_OUTER_RADIUS - 2) * 2, (APP_CLOCK_RING_OUTER_RADIUS - 2) * 2);
    lv_obj_align(inner_fill, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(inner_fill, lv_color_hex(APP_CLOCK_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(inner_fill, (lv_opa_t)(255 * 35 / 100), 0);
    lv_obj_set_style_border_width(inner_fill, 0, 0);
    lv_obj_set_style_radius(inner_fill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(inner_fill, 0, 0);

    lv_obj_t *clock_face = lv_obj_create(screen);
    lv_obj_set_size(clock_face, CLOCK_FACE_SIZE, CLOCK_FACE_SIZE);
    lv_obj_align(clock_face, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(clock_face, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_face, 0, 0);
    lv_obj_set_style_radius(clock_face, 0, 0);
    lv_obj_set_style_pad_all(clock_face, 0, 0);

    create_minute_ticks(clock_face);

    static const char *clock_numbers[] = {
        "12", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"
    };
    for (int i = 0; i < 12; i++) {
        float angle = (float)i * 2.0f * APP_CLOCK_PI / 12.0f;
        lv_obj_t *number = lv_label_create(clock_face);
        lv_label_set_text(number, clock_numbers[i]);
        lv_obj_set_style_text_color(number, lv_color_hex(APP_CLOCK_COLOR_INK_2), 0);
        lv_obj_set_style_text_align(number, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(number, 32);
        lv_obj_align(number, LV_ALIGN_TOP_LEFT,
                     CLOCK_CENTER - 16 + (int)lroundf(sinf(angle) * APP_CLOCK_NUMBER_RADIUS),
                     CLOCK_CENTER - 12 - (int)lroundf(cosf(angle) * APP_CLOCK_NUMBER_RADIUS));
    }

    /* 上方天气：图标单独一行，文字显示在图标下方 */
    lv_obj_t *weather_row = lv_obj_create(screen);
    lv_obj_set_style_bg_opa(weather_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(weather_row, 0, 0);
    lv_obj_set_style_pad_all(weather_row, 0, 0);
    lv_obj_set_flex_flow(weather_row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(weather_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(weather_row, 2, 0);
    lv_obj_set_width(weather_row, APP_CLOCK_WEATHER_ROW_WIDTH);
    lv_obj_set_height(weather_row, APP_CLOCK_WEATHER_ROW_HEIGHT);
    lv_obj_align(weather_row, LV_ALIGN_TOP_MID, 0, APP_CLOCK_WEATHER_ROW_Y);

    weather_icon = lv_image_create(weather_row);
    lv_image_set_src(weather_icon, weather_icons[WEATHER_SUNNY]);

    weather_label = lv_label_create(weather_row);
    lv_label_set_text(weather_label, APP_CLOCK_WEATHER_TEXT);
    lv_obj_set_width(weather_label, APP_CLOCK_WEATHER_TEXT_WIDTH);
    lv_label_set_long_mode(weather_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(weather_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(weather_label, lv_color_hex(APP_CLOCK_COLOR_INK_2), 0);
    lv_obj_set_style_text_font(weather_label, CJK_FONT, 0);

    time_period_label = lv_label_create(clock_face);
    lv_label_set_text(time_period_label, "上午");
    lv_obj_set_style_text_color(time_period_label, lv_color_hex(APP_CLOCK_COLOR_INK_3), 0);
    lv_obj_set_style_text_font(time_period_label, CJK_FONT, 0);
    lv_obj_set_style_text_align(time_period_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(time_period_label, APP_CLOCK_PERIOD_LABEL_WIDTH);
    lv_obj_align(time_period_label, LV_ALIGN_TOP_LEFT,
                 APP_CLOCK_PERIOD_LABEL_X, APP_CLOCK_PERIOD_LABEL_Y);


    hour_hand = create_clock_hand(clock_face, lv_color_hex(APP_CLOCK_COLOR_INK), 5);
    minute_hand = create_clock_hand(clock_face, lv_color_hex(APP_CLOCK_COLOR_INK), 4);
    second_hand = create_clock_hand(clock_face, lv_color_hex(APP_CLOCK_COLOR_BRAND), 2);

    /* 中心圆点：外层 brand 色 + 内层背景色 */
    lv_obj_t *center_outer = lv_obj_create(clock_face);
    lv_obj_set_size(center_outer, 9, 9);
    lv_obj_set_pos(center_outer, CLOCK_CENTER - 4, CLOCK_CENTER - 4);
    lv_obj_set_style_bg_color(center_outer, lv_color_hex(APP_CLOCK_COLOR_BRAND), 0);
    lv_obj_set_style_bg_opa(center_outer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(center_outer, 0, 0);
    lv_obj_set_style_radius(center_outer, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *center_inner = lv_obj_create(clock_face);
    lv_obj_set_size(center_inner, 4, 4);
    lv_obj_set_pos(center_inner, CLOCK_CENTER - 2, CLOCK_CENTER - 2);
    lv_obj_set_style_bg_color(center_inner, lv_color_hex(APP_CLOCK_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(center_inner, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(center_inner, 0, 0);
    lv_obj_set_style_radius(center_inner, LV_RADIUS_CIRCLE, 0);

    // 日期标签：屏幕下方、农历上方，居中，CJK 字体
    date_label = lv_label_create(screen);
    lv_obj_set_style_text_color(date_label, lv_color_hex(APP_CLOCK_COLOR_INK_2), 0);
    lv_obj_set_style_text_font(date_label, CJK_FONT, 0);
    lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(date_label, BSP_LCD_H_RES);
    lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, APP_CLOCK_DATE_LABEL_Y);

    /* 下方农历 */
    lunar_label = lv_label_create(screen);
    lv_label_set_text(lunar_label, "农历计算中");
    lv_obj_set_style_text_color(lunar_label, lv_color_hex(APP_CLOCK_COLOR_INK_3), 0);
    lv_obj_set_style_text_font(lunar_label, CJK_FONT, 0);
    lv_obj_set_style_text_align(lunar_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lunar_label, BSP_LCD_H_RES);
    lv_obj_align(lunar_label, LV_ALIGN_TOP_MID, 0, APP_CLOCK_LUNAR_LABEL_Y);

    clock_update_cb(NULL);
    lv_timer_create(clock_update_cb, 1000, NULL);
    lvgl_port_unlock();
    return ESP_OK;
}
