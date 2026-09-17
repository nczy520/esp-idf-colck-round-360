#include "timer_ui.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "bsp_display.h"

LV_FONT_DECLARE(lv_font_cjk_clock_18);

#define TIMER_MAX_MS             (99UL * 60UL * 60UL * 1000UL + 59UL * 60UL * 1000UL + 59UL * 1000UL + 999UL)
#define TIMER_WINDOW_SIZE        BSP_LCD_H_RES
#define TIMER_COLOR_PANEL        0x0B1220
#define TIMER_COLOR_BORDER       0x475569
#define TIMER_COLOR_TEXT         0xF9FAFB
#define TIMER_COLOR_MUTED        0x9CA3AF
#define TIMER_COLOR_ACCENT       0x0D9488
#define TIMER_COLOR_ACTION       0x1F2937
#define TIMER_COLOR_STOP         0xB91C1C

static lv_obj_t *timer_root;
static lv_obj_t *time_label;
static lv_obj_t *milliseconds_label;
static lv_obj_t *run_button;
static lv_obj_t *stop_button;
static lv_obj_t *clear_button;
static lv_obj_t *run_button_label;
static lv_obj_t *state_label;
static lv_timer_t *refresh_timer;
static uint32_t elapsed_ms;
static uint32_t run_started_at;

typedef enum {
    TIMER_STATE_IDLE,
    TIMER_STATE_RUNNING,
    TIMER_STATE_PAUSED,
    TIMER_STATE_STOPPED,
} timer_state_t;

static timer_state_t timer_state;

static void set_button_enabled(lv_obj_t *button, bool enabled)
{
    if (enabled) {
        lv_obj_remove_state(button, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(button, LV_STATE_DISABLED);
    }
}

static void update_controls(void)
{
    bool is_running = timer_state == TIMER_STATE_RUNNING;
    bool is_stopped = timer_state == TIMER_STATE_STOPPED;

    set_button_enabled(run_button, true);
    set_button_enabled(stop_button, timer_state != TIMER_STATE_IDLE && !is_stopped);
    set_button_enabled(clear_button, is_stopped);

    if (is_running) {
        lv_label_set_text(run_button_label, "暂停");
        lv_label_set_text(state_label, "计时中");
    } else if (timer_state == TIMER_STATE_PAUSED) {
        lv_label_set_text(run_button_label, "继续");
        lv_label_set_text(state_label, "已暂停");
    } else if (is_stopped) {
        lv_label_set_text(run_button_label, "开始");
        lv_label_set_text(state_label, "已停止");
    } else {
        lv_label_set_text(run_button_label, "开始");
        lv_label_set_text(state_label, "未开始");
    }
}

static void update_time_label(void)
{
    uint32_t display_ms = elapsed_ms;
    if (timer_state == TIMER_STATE_RUNNING) {
        uint32_t run_duration = lv_tick_elaps(run_started_at);
        display_ms += run_duration;
    }
    if (display_ms > TIMER_MAX_MS) {
        display_ms = TIMER_MAX_MS;
        timer_state = TIMER_STATE_STOPPED;
        elapsed_ms = display_ms;
        update_controls();
    }

    uint32_t milliseconds = display_ms % 1000;
    uint32_t total_seconds = display_ms / 1000;
    uint32_t seconds = total_seconds % 60;
    uint32_t total_minutes = total_seconds / 60;
    uint32_t minutes = total_minutes % 60;
    uint32_t hours = total_minutes / 60;
    lv_label_set_text_fmt(time_label, "%02lu:%02lu:%02lu",
                          (unsigned long)hours, (unsigned long)minutes,
                          (unsigned long)seconds);
    lv_label_set_text_fmt(milliseconds_label, ".%03lu", (unsigned long)milliseconds);
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    update_time_label();
}

static void style_button(lv_obj_t *button, lv_color_t color)
{
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_set_style_opa(button, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
}

static void run_clicked_cb(lv_event_t *event)
{
    (void)event;
    if (timer_state == TIMER_STATE_RUNNING) {
        elapsed_ms += lv_tick_elaps(run_started_at);
        timer_state = TIMER_STATE_PAUSED;
    } else if (elapsed_ms < TIMER_MAX_MS) {
        run_started_at = lv_tick_get();
        timer_state = TIMER_STATE_RUNNING;
    }
    update_controls();
    update_time_label();
}

static void stop_clicked_cb(lv_event_t *event)
{
    (void)event;
    if (timer_state == TIMER_STATE_RUNNING) {
        elapsed_ms += lv_tick_elaps(run_started_at);
    }
    timer_state = TIMER_STATE_STOPPED;
    update_controls();
    update_time_label();
}

static void clear_clicked_cb(lv_event_t *event)
{
    (void)event;
    elapsed_ms = 0;
    update_time_label();
}

static void close_clicked_cb(lv_event_t *event)
{
    (void)event;
    if (refresh_timer) {
        lv_timer_delete(refresh_timer);
        refresh_timer = NULL;
    }
    lv_obj_delete(timer_root);
    timer_root = NULL;
    time_label = NULL;
    milliseconds_label = NULL;
    run_button = NULL;
    stop_button = NULL;
    clear_button = NULL;
    run_button_label = NULL;
    state_label = NULL;
    timer_state = TIMER_STATE_IDLE;
}

void timer_ui_show(void)
{
    if (timer_root) {
        return;
    }

    lvgl_port_lock(0);
    lv_obj_t *screen = lv_screen_active();
    timer_root = lv_obj_create(screen);
    lv_obj_set_size(timer_root, TIMER_WINDOW_SIZE, TIMER_WINDOW_SIZE);
    lv_obj_set_pos(timer_root, 0, 0);
    lv_obj_set_style_bg_color(timer_root, lv_color_hex(TIMER_COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(timer_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(timer_root, lv_color_hex(TIMER_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(timer_root, 2, 0);
    lv_obj_set_style_radius(timer_root, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(timer_root, 0, 0);
    lv_obj_clear_flag(timer_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dial_ring = lv_obj_create(timer_root);
    lv_obj_set_size(dial_ring, 286, 286);
    lv_obj_align(dial_ring, LV_ALIGN_CENTER, 0, 4);
    lv_obj_set_style_bg_opa(dial_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(dial_ring, lv_color_hex(TIMER_COLOR_BORDER), 0);
    lv_obj_set_style_border_opa(dial_ring, LV_OPA_50, 0);
    lv_obj_set_style_border_width(dial_ring, 1, 0);
    lv_obj_set_style_radius(dial_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(dial_ring, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(timer_root);
    lv_label_set_text(title, "计时器");
    lv_obj_set_style_text_color(title, lv_color_hex(TIMER_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(title, &lv_font_cjk_clock_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 62);

    state_label = lv_label_create(timer_root);
    lv_label_set_text(state_label, "已停止");
    lv_obj_set_style_text_color(state_label, lv_color_hex(TIMER_COLOR_MUTED), 0);
    lv_obj_set_style_text_font(state_label, &lv_font_cjk_clock_18, 0);
    lv_obj_align(state_label, LV_ALIGN_TOP_MID, 0, 92);

    lv_obj_t *time_row = lv_obj_create(timer_root);
    lv_obj_set_size(time_row, TIMER_WINDOW_SIZE - 12, 64);
    lv_obj_align(time_row, LV_ALIGN_CENTER, 0, -4);
    lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(time_row, 0, 0);
    lv_obj_set_style_pad_all(time_row, 0, 0);
    lv_obj_clear_flag(time_row, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    time_label = lv_label_create(time_row);
    lv_obj_set_size(time_label, 258, 64);
    lv_obj_set_pos(time_label, 0, 4);
    lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(TIMER_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_40, 0);

    milliseconds_label = lv_label_create(time_row);
    lv_obj_set_size(milliseconds_label, 78, 28);
    lv_obj_set_pos(milliseconds_label, 264, 25);
    lv_obj_set_style_text_align(milliseconds_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(milliseconds_label, lv_color_hex(TIMER_COLOR_MUTED), 0);
    lv_obj_set_style_text_font(milliseconds_label, &lv_font_montserrat_20, 0);

    run_button = lv_button_create(timer_root);
    lv_obj_set_size(run_button, 64, 64);
    lv_obj_set_pos(run_button, 52, 244);
    style_button(run_button, lv_color_hex(TIMER_COLOR_ACCENT));
    lv_obj_add_event_cb(run_button, run_clicked_cb, LV_EVENT_CLICKED, NULL);
    run_button_label = lv_label_create(run_button);
    lv_label_set_text(run_button_label, "开始");
    lv_obj_set_style_text_color(run_button_label, lv_color_hex(TIMER_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(run_button_label, &lv_font_cjk_clock_18, 0);
    lv_obj_align(run_button_label, LV_ALIGN_CENTER, 0, 0);

    stop_button = lv_button_create(timer_root);
    lv_obj_set_size(stop_button, 64, 64);
    lv_obj_set_pos(stop_button, 148, 244);
    style_button(stop_button, lv_color_hex(TIMER_COLOR_STOP));
    lv_obj_add_event_cb(stop_button, stop_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *stop_label = lv_label_create(stop_button);
    lv_label_set_text(stop_label, "停止");
    lv_obj_set_style_text_color(stop_label, lv_color_hex(TIMER_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(stop_label, &lv_font_cjk_clock_18, 0);
    lv_obj_align(stop_label, LV_ALIGN_CENTER, 0, 0);

    clear_button = lv_button_create(timer_root);
    lv_obj_set_size(clear_button, 64, 64);
    lv_obj_set_pos(clear_button, 244, 244);
    style_button(clear_button, lv_color_hex(TIMER_COLOR_ACTION));
    lv_obj_add_event_cb(clear_button, clear_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clear_label = lv_label_create(clear_button);
    lv_label_set_text(clear_label, "清零");
    lv_obj_set_style_text_color(clear_label, lv_color_hex(TIMER_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(clear_label, &lv_font_cjk_clock_18, 0);
    lv_obj_align(clear_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *close_button = lv_button_create(timer_root);
    lv_obj_set_size(close_button, 42, 42);
    lv_obj_set_pos(close_button, 255, 57);
    style_button(close_button, lv_color_hex(TIMER_COLOR_ACTION));
    lv_obj_add_event_cb(close_button, close_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_label = lv_label_create(close_button);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_label, lv_color_hex(TIMER_COLOR_TEXT), 0);
    lv_obj_align(close_label, LV_ALIGN_CENTER, 0, 0);

    elapsed_ms = 0;
    timer_state = TIMER_STATE_IDLE;
    update_controls();
    update_time_label();
    refresh_timer = lv_timer_create(refresh_timer_cb, 10, NULL);
    lvgl_port_unlock();
}