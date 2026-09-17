#include "countdown_ui.h"

#include <stdio.h>
#include <stdint.h>

#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "bsp_display.h"

LV_FONT_DECLARE(lv_font_cjk_clock_18);

#define COUNTDOWN_WINDOW_SIZE BSP_LCD_H_RES
#define COUNTDOWN_COLOR_PANEL 0x0B1220
#define COUNTDOWN_COLOR_BORDER 0x475569
#define COUNTDOWN_COLOR_TEXT 0xF9FAFB
#define COUNTDOWN_COLOR_MUTED 0x9CA3AF
#define COUNTDOWN_COLOR_ACCENT 0x0D9488
#define COUNTDOWN_COLOR_ACTION 0x1F2937
#define COUNTDOWN_MAX_HOURS 99

typedef enum {
    COUNTDOWN_STATE_SETTING,
    COUNTDOWN_STATE_RUNNING,
    COUNTDOWN_STATE_PAUSED,
    COUNTDOWN_STATE_FINISHED,
} countdown_state_t;

static lv_obj_t *countdown_root;
static lv_obj_t *time_label;
static lv_obj_t *state_label;
static lv_obj_t *primary_label;
static lv_obj_t *setting_row;
static lv_obj_t *primary_button;
static lv_obj_t *hour_roller;
static lv_obj_t *minute_roller;
static lv_obj_t *second_roller;
static lv_timer_t *refresh_timer;
static uint32_t configured_ms;
static uint32_t remaining_ms;
static uint32_t run_started_at;
static countdown_state_t countdown_state;
static bool countdown_initialized;
static char hour_options[COUNTDOWN_MAX_HOURS * 3 + 3];
static char minute_second_options[60 * 3];

static void style_button(lv_obj_t *button, lv_color_t color)
{
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_set_scrollable(button, false);
}

static void update_time_label(void)
{
    uint32_t display_ms = remaining_ms;
    if (countdown_state == COUNTDOWN_STATE_RUNNING) {
        uint32_t elapsed = lv_tick_elaps(run_started_at);
        display_ms = elapsed >= remaining_ms ? 0 : remaining_ms - elapsed;
        if (display_ms == 0) {
            remaining_ms = 0;
            countdown_state = COUNTDOWN_STATE_FINISHED;
        }
    }

    uint32_t total_seconds = display_ms / 1000;
    lv_label_set_text_fmt(time_label, "%02lu:%02lu:%02lu",
                          (unsigned long)(total_seconds / 3600),
                          (unsigned long)((total_seconds / 60) % 60),
                          (unsigned long)(total_seconds % 60));

    if (countdown_state == COUNTDOWN_STATE_SETTING) {
        lv_label_set_text(state_label, "设置倒计时");
        lv_label_set_text(primary_label, "开始");
    } else if (countdown_state == COUNTDOWN_STATE_RUNNING) {
        lv_label_set_text(state_label, "倒计时中");
        lv_label_set_text(primary_label, "暂停");
    } else if (countdown_state == COUNTDOWN_STATE_PAUSED) {
        lv_label_set_text(state_label, "已暂停");
        lv_label_set_text(primary_label, "继续");
    } else {
        lv_label_set_text(state_label, "时间到");
        lv_label_set_text(primary_label, "重新开始");
    }
}

static void sync_rollers_from_time(uint32_t time_ms)
{
    uint32_t total_seconds = time_ms / 1000;
    lv_roller_set_selected(hour_roller, total_seconds / 3600, LV_ANIM_OFF);
    lv_roller_set_selected(minute_roller, (total_seconds / 60) % 60, LV_ANIM_OFF);
    lv_roller_set_selected(second_roller, total_seconds % 60, LV_ANIM_OFF);
}

static void sync_time_from_rollers(void)
{
    uint32_t selected_hours = lv_roller_get_selected(hour_roller);
    uint32_t selected_minutes = lv_roller_get_selected(minute_roller);
    uint32_t selected_seconds = lv_roller_get_selected(second_roller);
    configured_ms = (selected_hours * 3600 + selected_minutes * 60 + selected_seconds) * 1000;
    remaining_ms = configured_ms;
}

static void roller_changed_cb(lv_event_t *event)
{
    (void)event;
    sync_time_from_rollers();
    update_time_label();
}

static void primary_clicked_cb(lv_event_t *event)
{
    (void)event;
    if (countdown_state == COUNTDOWN_STATE_SETTING || countdown_state == COUNTDOWN_STATE_FINISHED) {
        sync_time_from_rollers();
        if (remaining_ms == 0) {
            return;
        }
        countdown_state = COUNTDOWN_STATE_RUNNING;
        run_started_at = lv_tick_get();
        lv_obj_set_hidden(setting_row, true);
        lv_obj_set_hidden(time_label, false);
    } else if (countdown_state == COUNTDOWN_STATE_RUNNING) {
        uint32_t elapsed = lv_tick_elaps(run_started_at);
        remaining_ms = elapsed >= remaining_ms ? 0 : remaining_ms - elapsed;
        countdown_state = COUNTDOWN_STATE_PAUSED;
    } else {
        countdown_state = COUNTDOWN_STATE_RUNNING;
        run_started_at = lv_tick_get();
    }
    update_time_label();
}

static void reset_clicked_cb(lv_event_t *event)
{
    (void)event;
    countdown_state = COUNTDOWN_STATE_SETTING;
    configured_ms = remaining_ms;
    sync_rollers_from_time(configured_ms);
    lv_obj_set_hidden(time_label, true);
    lv_obj_set_hidden(setting_row, false);
    update_time_label();
}

static void close_clicked_cb(lv_event_t *event)
{
    (void)event;
    if (refresh_timer) {
        lv_timer_delete(refresh_timer);
        refresh_timer = NULL;
    }
    lv_obj_delete(countdown_root);
    countdown_root = NULL;
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    update_time_label();
}

static void create_roller_options(char *options, uint32_t maximum)
{
    char *cursor = options;
    for (uint32_t value = 0; value <= maximum; value++) {
        cursor += snprintf(cursor, 4, value == maximum ? "%02lu" : "%02lu\n",
                           (unsigned long)value);
    }
}

static lv_obj_t *create_time_roller(lv_obj_t *parent, int x, const char *options)
{
    lv_obj_t *roller = lv_roller_create(parent);
    lv_obj_set_size(roller, 70, 126);
    lv_obj_set_pos(roller, x, 0);
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller, 3);
    lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_hex(COUNTDOWN_COLOR_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_hex(COUNTDOWN_COLOR_TEXT), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(roller, lv_color_hex(COUNTDOWN_COLOR_ACTION), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_80, LV_PART_SELECTED);
    lv_obj_set_style_radius(roller, 6, LV_PART_SELECTED);
    lv_obj_add_event_cb(roller, roller_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    return roller;
}

void countdown_ui_show(void)
{
    if (countdown_root) {
        return;
    }

    lvgl_port_lock(0);
    countdown_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(countdown_root, COUNTDOWN_WINDOW_SIZE, COUNTDOWN_WINDOW_SIZE);
    lv_obj_set_pos(countdown_root, 0, 0);
    lv_obj_set_style_bg_color(countdown_root, lv_color_hex(COUNTDOWN_COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(countdown_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(countdown_root, lv_color_hex(COUNTDOWN_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(countdown_root, 2, 0);
    lv_obj_set_style_radius(countdown_root, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(countdown_root, 0, 0);
    lv_obj_set_scrollable(countdown_root, false);

    lv_obj_t *dial_ring = lv_obj_create(countdown_root);
    lv_obj_set_size(dial_ring, 286, 286);
    lv_obj_align(dial_ring, LV_ALIGN_CENTER, 0, 4);
    lv_obj_set_style_bg_opa(dial_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(dial_ring, lv_color_hex(COUNTDOWN_COLOR_BORDER), 0);
    lv_obj_set_style_border_opa(dial_ring, LV_OPA_50, 0);
    lv_obj_set_style_border_width(dial_ring, 1, 0);
    lv_obj_set_style_radius(dial_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_clickable(dial_ring, false);
    lv_obj_set_scrollable(dial_ring, false);

    lv_obj_t *title = lv_label_create(countdown_root);
    lv_label_set_text(title, "倒计时");
    lv_obj_set_style_text_color(title, lv_color_hex(COUNTDOWN_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(title, &lv_font_cjk_clock_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 62);

    state_label = lv_label_create(countdown_root);
    lv_obj_set_style_text_color(state_label, lv_color_hex(COUNTDOWN_COLOR_MUTED), 0);
    lv_obj_set_style_text_font(state_label, &lv_font_cjk_clock_18, 0);
    lv_obj_align(state_label, LV_ALIGN_TOP_MID, 0, 88);

    time_label = lv_label_create(countdown_root);
    lv_obj_set_width(time_label, COUNTDOWN_WINDOW_SIZE - 24);
    lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(COUNTDOWN_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_hidden(time_label, true);

    setting_row = lv_obj_create(countdown_root);
    lv_obj_set_size(setting_row, 250, 126);
    lv_obj_align(setting_row, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(setting_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(setting_row, 0, 0);
    lv_obj_set_style_pad_all(setting_row, 0, 0);
    lv_obj_set_clickable(setting_row, false);
    lv_obj_set_scrollable(setting_row, false);
    create_roller_options(hour_options, COUNTDOWN_MAX_HOURS);
    create_roller_options(minute_second_options, 59);
    hour_roller = create_time_roller(setting_row, 8, hour_options);
    minute_roller = create_time_roller(setting_row, 90, minute_second_options);
    second_roller = create_time_roller(setting_row, 172, minute_second_options);

    primary_button = lv_button_create(countdown_root);
    lv_obj_set_size(primary_button, 64, 64);
    lv_obj_set_pos(primary_button, 92, 266);
    style_button(primary_button, lv_color_hex(COUNTDOWN_COLOR_ACCENT));
    lv_obj_add_event_cb(primary_button, primary_clicked_cb, LV_EVENT_CLICKED, NULL);
    primary_label = lv_label_create(primary_button);
    lv_obj_set_style_text_color(primary_label, lv_color_hex(COUNTDOWN_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(primary_label, &lv_font_cjk_clock_18, 0);
    lv_obj_align(primary_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *reset_button = lv_button_create(countdown_root);
    lv_obj_set_size(reset_button, 64, 64);
    lv_obj_set_pos(reset_button, 204, 266);
    style_button(reset_button, lv_color_hex(COUNTDOWN_COLOR_ACTION));
    lv_obj_add_event_cb(reset_button, reset_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *reset_label = lv_label_create(reset_button);
    lv_label_set_text(reset_label, "重设");
    lv_obj_set_style_text_color(reset_label, lv_color_hex(COUNTDOWN_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(reset_label, &lv_font_cjk_clock_18, 0);
    lv_obj_align(reset_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *close_button = lv_button_create(countdown_root);
    lv_obj_set_size(close_button, 42, 42);
    lv_obj_set_pos(close_button, 255, 57);
    style_button(close_button, lv_color_hex(COUNTDOWN_COLOR_ACTION));
    lv_obj_add_event_cb(close_button, close_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_label = lv_label_create(close_button);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_label, lv_color_hex(COUNTDOWN_COLOR_TEXT), 0);
    lv_obj_align(close_label, LV_ALIGN_CENTER, 0, 0);

    if (!countdown_initialized) {
        configured_ms = 0;
        remaining_ms = 0;
        countdown_state = COUNTDOWN_STATE_SETTING;
        countdown_initialized = true;
    }
    if (countdown_state == COUNTDOWN_STATE_SETTING) {
        sync_rollers_from_time(configured_ms);
        lv_obj_set_hidden(time_label, true);
        lv_obj_set_hidden(setting_row, false);
    } else {
        lv_obj_set_hidden(time_label, false);
        lv_obj_set_hidden(setting_row, true);
    }
    update_time_label();
    refresh_timer = lv_timer_create(refresh_timer_cb, 100, NULL);
    lvgl_port_unlock();
}