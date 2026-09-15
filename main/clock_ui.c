#include "clock_ui.h"

#include <math.h>
#include <time.h>

#include "esp_check.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "bsp_display.h"
#include "calendar_lunar.h"
#include "weather_icons.h"

/* 定制 CJK 字体：18px，含二十四节气 / 天干地支 / 农历日期 / 天气 */
LV_FONT_DECLARE(lv_font_cjk_clock_18);

/* Clock design tokens — matching LVGL 时钟表盘 design */
#define COLOR_CLOCK_BG           0xFFFFFF
#define COLOR_CLOCK_SURFACE      0xF9FAFB
#define COLOR_CLOCK_INK          0x1F2937
#define COLOR_CLOCK_INK_2        0x6B7280
#define COLOR_CLOCK_INK_3        0x9CA3AF
#define COLOR_CLOCK_LINE         0xE5E7EB
#define COLOR_CLOCK_BRAND        0x0D9488

#define CLOCK_FACE_SIZE          BSP_LCD_H_RES
#define CLOCK_CENTER             (CLOCK_FACE_SIZE / 2)
#define CLOCK_PI                 3.14159265358979323846f
#define CLOCK_HOUR_HAND_LENGTH   80
#define CLOCK_MINUTE_HAND_LENGTH 109
#define CLOCK_SECOND_HAND_LENGTH 124
#define CLOCK_SECOND_TAIL_LENGTH 15
#define CLOCK_TICK_MAJOR_OUTER   175
#define CLOCK_TICK_MAJOR_INNER   158
#define CLOCK_TICK_MINOR_OUTER   171
#define CLOCK_TICK_MINOR_INNER   162
#define CLOCK_RING_OUTER_RADIUS  180
#define CLOCK_NUMBER_RADIUS      144
#define WEATHER_TEXT             "晴朗 26°C"
#define CJK_FONT                 (&lv_font_cjk_clock_18)

static const char *TAG = "clock_ui";
static lv_obj_t *date_label;
static lv_obj_t *lunar_label;
static lv_obj_t *hour_hand;
static lv_obj_t *minute_hand;
static lv_obj_t *second_hand;
static lv_point_precise_t hour_points[2];
static lv_point_precise_t minute_points[2];
static lv_point_precise_t second_points[2];
static lv_point_precise_t minute_tick_points[60][2];

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
        float angle = (float)i * 2.0f * CLOCK_PI / 60.0f - CLOCK_PI / 2.0f;
        bool major = (i % 15) == 0;
        bool medium = !major && (i % 5) == 0;
        int outer_radius = major ? CLOCK_TICK_MAJOR_OUTER : CLOCK_TICK_MINOR_OUTER;
        int inner_radius = major ? CLOCK_TICK_MAJOR_INNER : CLOCK_TICK_MINOR_INNER;
        minute_tick_points[i][0].x = CLOCK_CENTER + (lv_coord_t)lroundf(cosf(angle) * inner_radius);
        minute_tick_points[i][0].y = CLOCK_CENTER + (lv_coord_t)lroundf(sinf(angle) * inner_radius);
        minute_tick_points[i][1].x = CLOCK_CENTER + (lv_coord_t)lroundf(cosf(angle) * outer_radius);
        minute_tick_points[i][1].y = CLOCK_CENTER + (lv_coord_t)lroundf(sinf(angle) * outer_radius);

        lv_obj_t *tick = lv_line_create(parent);
        lv_obj_set_style_line_color(tick,
            lv_color_hex((major || medium) ? COLOR_CLOCK_INK_2 : COLOR_CLOCK_INK_3), 0);
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
    calendar_info_t calendar;
    bool calendar_valid = calendar_get_info(&current_time, &calendar);

    static const char *weekday_names[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};

    float second_angle = (float)current_time.tm_sec * 2.0f * CLOCK_PI / 60.0f - CLOCK_PI / 2.0f;
    float minute_angle = ((float)current_time.tm_min + (float)current_time.tm_sec / 60.0f)
                         * 2.0f * CLOCK_PI / 60.0f - CLOCK_PI / 2.0f;
    float hour_angle = ((float)(current_time.tm_hour % 12) + (float)current_time.tm_min / 60.0f)
                       * 2.0f * CLOCK_PI / 12.0f - CLOCK_PI / 2.0f;

    update_clock_hand(hour_hand, hour_points, hour_angle, CLOCK_HOUR_HAND_LENGTH, 0);
    update_clock_hand(minute_hand, minute_points, minute_angle, CLOCK_MINUTE_HAND_LENGTH, 0);
    update_clock_hand(second_hand, second_points, second_angle,
                      CLOCK_SECOND_HAND_LENGTH, CLOCK_SECOND_TAIL_LENGTH);
    lv_label_set_text_fmt(date_label, "%d月%d日 %s",
                          current_time.tm_mon + 1,
                          current_time.tm_mday,
                          weekday_names[current_time.tm_wday]);
    if (calendar_valid) {
        lv_label_set_text_fmt(lunar_label, "农历%s%s  %s\n节日%s  节气%s\n干支 %s %s %s",
                              calendar.lunar.leap_month ? "闰" : "",
                              calendar.lunar_month_name, calendar.lunar_day_name,
                      calendar.festival, calendar.solar_term,
                              calendar.year_ganzhi, calendar.month_ganzhi, calendar.day_ganzhi);
    }
}

esp_err_t clock_ui_create(void)
{
    lvgl_port_lock(0);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_CLOCK_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    /* 外缘装饰环：r=180，线色描边 */
    lv_obj_t *outer_ring = lv_obj_create(screen);
    lv_obj_set_size(outer_ring, CLOCK_RING_OUTER_RADIUS * 2, CLOCK_RING_OUTER_RADIUS * 2);
    lv_obj_align(outer_ring, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(outer_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(outer_ring, lv_color_hex(COLOR_CLOCK_LINE), 0);
    lv_obj_set_style_border_width(outer_ring, 1, 0);
    lv_obj_set_style_radius(outer_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(outer_ring, 0, 0);

    /* 内侧浅色填充：surface 35% 不透明度 */
    lv_obj_t *inner_fill = lv_obj_create(screen);
    lv_obj_set_size(inner_fill, (CLOCK_RING_OUTER_RADIUS - 2) * 2, (CLOCK_RING_OUTER_RADIUS - 2) * 2);
    lv_obj_align(inner_fill, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(inner_fill, lv_color_hex(COLOR_CLOCK_SURFACE), 0);
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
        float angle = (float)i * 2.0f * CLOCK_PI / 12.0f;
        lv_obj_t *number = lv_label_create(clock_face);
        lv_label_set_text(number, clock_numbers[i]);
        lv_obj_set_style_text_color(number, lv_color_hex(COLOR_CLOCK_INK_2), 0);
        lv_obj_set_style_text_align(number, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(number, 32);
        lv_obj_align(number, LV_ALIGN_TOP_LEFT,
                     CLOCK_CENTER - 16 + (int)lroundf(sinf(angle) * CLOCK_NUMBER_RADIUS),
                     CLOCK_CENTER - 12 - (int)lroundf(cosf(angle) * CLOCK_NUMBER_RADIUS));
    }

    /* 上方天气：图标 + 文本，ink-2 颜色，CJK 字体 */
    lv_obj_t *weather_row = lv_obj_create(screen);
    lv_obj_set_style_bg_opa(weather_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(weather_row, 0, 0);
    lv_obj_set_style_pad_all(weather_row, 0, 0);
    lv_obj_set_flex_flow(weather_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(weather_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(weather_row, 4, 0);
    lv_obj_set_height(weather_row, 20);
    lv_obj_align(weather_row, LV_ALIGN_TOP_MID, 0, 100);

    lv_obj_t *weather_icon = lv_image_create(weather_row);
    lv_image_set_src(weather_icon, weather_icons[WEATHER_SUNNY]);

    lv_obj_t *weather_label = lv_label_create(weather_row);
    lv_label_set_text(weather_label, WEATHER_TEXT);
    lv_obj_set_style_text_color(weather_label, lv_color_hex(COLOR_CLOCK_INK_2), 0);
    lv_obj_set_style_text_font(weather_label, CJK_FONT, 0);

    hour_hand = create_clock_hand(clock_face, lv_color_hex(COLOR_CLOCK_INK), 5);
    minute_hand = create_clock_hand(clock_face, lv_color_hex(COLOR_CLOCK_INK), 4);
    second_hand = create_clock_hand(clock_face, lv_color_hex(COLOR_CLOCK_BRAND), 2);

    /* 中心圆点：外层 brand 色 + 内层背景色 */
    lv_obj_t *center_outer = lv_obj_create(clock_face);
    lv_obj_set_size(center_outer, 9, 9);
    lv_obj_set_pos(center_outer, CLOCK_CENTER - 4, CLOCK_CENTER - 4);
    lv_obj_set_style_bg_color(center_outer, lv_color_hex(COLOR_CLOCK_BRAND), 0);
    lv_obj_set_style_bg_opa(center_outer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(center_outer, 0, 0);
    lv_obj_set_style_radius(center_outer, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *center_inner = lv_obj_create(clock_face);
    lv_obj_set_size(center_inner, 4, 4);
    lv_obj_set_pos(center_inner, CLOCK_CENTER - 2, CLOCK_CENTER - 2);
    lv_obj_set_style_bg_color(center_inner, lv_color_hex(COLOR_CLOCK_BG), 0);
    lv_obj_set_style_bg_opa(center_inner, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(center_inner, 0, 0);
    lv_obj_set_style_radius(center_inner, LV_RADIUS_CIRCLE, 0);

    /* 下方日期 + 农历 */
    date_label = lv_label_create(screen);
    lv_obj_set_style_text_color(date_label, lv_color_hex(COLOR_CLOCK_INK_2), 0);
    lv_obj_set_style_text_font(date_label, CJK_FONT, 0);
    lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(date_label, BSP_LCD_H_RES);
    lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 220);

    lunar_label = lv_label_create(screen);
    lv_label_set_text(lunar_label, "农历计算中");
    lv_obj_set_style_text_color(lunar_label, lv_color_hex(COLOR_CLOCK_INK_3), 0);
    lv_obj_set_style_text_font(lunar_label, CJK_FONT, 0);
    lv_obj_set_style_text_align(lunar_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lunar_label, BSP_LCD_H_RES);
    lv_obj_align(lunar_label, LV_ALIGN_TOP_MID, 0, 251);

    clock_update_cb(NULL);
    lv_timer_create(clock_update_cb, 1000, NULL);
    lvgl_port_unlock();
    return ESP_OK;
}
