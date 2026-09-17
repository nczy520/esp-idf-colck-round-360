#include "menu_ui.h"

#include <math.h>

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "app_config.h"
#include "bsp_display.h"
#include "clock_ui.h"

extern const lv_image_dsc_t timer;
extern const lv_image_dsc_t stopwatch;
extern const lv_image_dsc_t lunar;
extern const lv_image_dsc_t settings;
extern const lv_image_dsc_t debug;
extern const lv_image_dsc_t device;
extern const lv_image_dsc_t weather;
extern const lv_image_dsc_t compass;

enum {
    MENU_ICON_TIMER,
    MENU_ICON_STOPWATCH,
    MENU_ICON_LUNAR,
    MENU_ICON_SETTINGS,
    MENU_ICON_DEBUG,
    MENU_ICON_DEVICE,
    MENU_ICON_WEATHER,
    MENU_ICON_COMPASS,
};

static const lv_image_dsc_t *menu_icons[] = {
    &timer,
    &stopwatch,
    &lunar,
    &settings,
    &debug,
    &device,
    &weather,
    &compass,
};

static const char *TAG = "menu_ui";

/* 弹出菜单设计参数 —— 深色圆盘 + 白色线性图标 + 蓝色高亮（参考设计图） */
#define MENU_OVERLAY_SIZE_X       BSP_LCD_H_RES       /* 全屏触摸叠加层 */
#define MENU_OVERLAY_SIZE_Y       BSP_LCD_V_RES
#define MENU_DIAMETER              (BSP_LCD_H_RES * 8 / 10)   /* 屏幕的 80% */
#define MENU_RADIUS                (MENU_DIAMETER / 2)
#define MENU_CENTER_COORD           (MENU_DIAMETER / 2)
#define MENU_BUTTON_ICON_SIZE       40                  /* 图标显示尺寸 */
#define MENU_BUTTON_BOX_SIZE        60                  /* 圆周按钮容器尺寸 */
#define MENU_BUTTON_BOX_PAD         ((MENU_BUTTON_BOX_SIZE - MENU_BUTTON_ICON_SIZE) / 2)
#define MENU_BUTTON_RING_RADIUS     (MENU_RADIUS * 3 / 4) /* 圆周按钮中心距菜单中心的距离 */
#define MENU_INNER_DIAMETER          (MENU_DIAMETER / 2)
#define MENU_CENTER_BUTTON_ICON_SIZE 40
#define MENU_CENTER_BUTTON_BOX_SIZE 60
#define MENU_DIM_OPA               153                  /* 背景遮罩 ~60% 透明度 */

/* 配色：深色圆盘 + 白色线性图标 + 蓝色高亮 */
#define MENU_COLOR_PANEL            0x000000            /* 菜单外圆环底色 */
#define MENU_COLOR_PANEL_BORDER     0x3A3A3C            /* 圆环描边 */
#define MENU_COLOR_INNER_PANEL      0x1C1C1E            /* 菜单内圆底色 */
#define MENU_COLOR_ICON             0xFFFFFF            /* 图标统一白色（线性风格） */
#define MENU_COLOR_ACCENT           0x3B82F6            /* 按压高亮蓝色 */
#define MENU_COLOR_CENTER_BG        0x2C2C2E            /* 中心按钮底色 */

static lv_obj_t *overlay;        /* 全屏透明触摸叠加层：捕获长按 */
static lv_obj_t *menu_root;      /* 当前弹出的菜单容器（NULL 表示菜单未显示 */

static void long_press_event_cb(lv_event_t *e);
static void button_clicked_cb(lv_event_t *e);
static void close_clicked_cb(lv_event_t *e);

/* 圆周功能按钮：透明底 + 白色线性图标，按压时显示蓝色圆形高亮 */
static void style_menu_button(lv_obj_t *btn)
{
    /* 默认态：透明底，无边框，图标直接落在深色圆盘上 */
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, MENU_BUTTON_BOX_PAD, LV_PART_MAIN);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(btn, true, LV_PART_MAIN);

    /* 按压态：蓝色圆形高亮背景（参考设计图底部蓝色选中态） */
    lv_obj_set_style_bg_color(btn, lv_color_hex(MENU_COLOR_ACCENT), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);

    /* 平滑过渡：按压/释放切换背景 */
    static const lv_style_prop_t transition_props[] = {
        LV_STYLE_BG_OPA, LV_STYLE_BG_COLOR, 0
    };
    static lv_style_transition_dsc_t transition_dsc;
    static bool transition_inited = false;
    if (!transition_inited) {
        lv_style_transition_dsc_init(&transition_dsc, transition_props, lv_anim_path_ease_out, 120, 0, NULL);
        transition_inited = true;
    }
    lv_obj_set_style_transition(btn, &transition_dsc, LV_PART_MAIN);

    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
}

/* 将图标统一重着色为白色，实现线性风格 */
static void style_icon_white(lv_obj_t *icon)
{
    lv_obj_set_style_img_recolor(icon, lv_color_hex(MENU_COLOR_ICON), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
}

esp_err_t menu_ui_init(void)
{
    lvgl_port_lock(0);
    lv_obj_t *screen = lv_screen_active();

    /* 全屏透明叠加层：在 clock_ui 之上捕获长按 */
    overlay = lv_obj_create(screen);
    lv_obj_set_size(overlay, MENU_OVERLAY_SIZE_X, MENU_OVERLAY_SIZE_Y);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    /* 默认即 LV_OBJ_FLAG_CLICKABLE，确保能接收长按事件 */
    lv_obj_add_event_cb(overlay, long_press_event_cb, LV_EVENT_LONG_PRESSED, NULL);

    lvgl_port_unlock();
    ESP_LOGI(TAG, "menu overlay installed (%dx%d)", MENU_OVERLAY_SIZE_X, MENU_OVERLAY_SIZE_Y);
    return ESP_OK;
}

void menu_ui_show(void)
{
    if (menu_root) {
        return;  /* 菜单已弹出 */
    }
    lvgl_port_lock(0);
    clock_ui_pause();

    /* 1. 半透明背景遮罩：仅拦截触摸，菜单只能由中心按钮关闭 */
    lv_obj_t *dim = lv_obj_create(overlay);
    lv_obj_set_size(dim, MENU_OVERLAY_SIZE_X, MENU_OVERLAY_SIZE_Y);
    lv_obj_set_pos(dim, 0, 0);
    lv_obj_set_style_bg_color(dim, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(dim, MENU_DIM_OPA, 0);
    lv_obj_set_style_border_width(dim, 0, 0);
    lv_obj_set_style_radius(dim, 0, 0);
    lv_obj_set_style_pad_all(dim, 0, 0);
    lv_obj_clear_flag(dim, LV_OBJ_FLAG_SCROLLABLE);

    /* 2. 不透明黑色圆环 */
    menu_root = lv_obj_create(overlay);
    lv_obj_set_size(menu_root, MENU_DIAMETER, MENU_DIAMETER);
    lv_obj_align(menu_root, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(menu_root, lv_color_hex(MENU_COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(menu_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(menu_root, lv_color_hex(MENU_COLOR_PANEL_BORDER), 0);
    lv_obj_set_style_border_width(menu_root, 1, 0);
    lv_obj_set_style_radius(menu_root, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(menu_root, 0, 0);
    lv_obj_clear_flag(menu_root, LV_OBJ_FLAG_SCROLLABLE);

    /* 3. 内层圆盘，形成外环与中心控制区两个层次 */
    lv_obj_t *inner_panel = lv_obj_create(menu_root);
    lv_obj_set_size(inner_panel, MENU_INNER_DIAMETER, MENU_INNER_DIAMETER);
    lv_obj_align(inner_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(inner_panel, lv_color_hex(MENU_COLOR_INNER_PANEL), 0);
    lv_obj_set_style_bg_opa(inner_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(inner_panel, 0, 0);
    lv_obj_set_style_radius(inner_panel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(inner_panel, 0, 0);
    lv_obj_clear_flag(inner_panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* 4. 圆环上的 8 个功能按钮（顺时针从 12 点钟方向开始） */
    for (int i = 0; i < 8; i++) {
        /* 角度从 -π/2 开始（12 点方向），每按钮增加 π/4 */
        float angle = (float)i * 2.0f * APP_CLOCK_PI / 8.0f - APP_CLOCK_PI / 2.0f;
        int cx = MENU_CENTER_COORD + (int)lroundf(cosf(angle) * MENU_BUTTON_RING_RADIUS);
        int cy = MENU_CENTER_COORD + (int)lroundf(sinf(angle) * MENU_BUTTON_RING_RADIUS);
        int x = cx - MENU_BUTTON_BOX_SIZE / 2;
        int y = cy - MENU_BUTTON_BOX_SIZE / 2;

        /* 按钮容器：透明底 + 按压蓝色高亮 */
        lv_obj_t *btn = lv_obj_create(menu_root);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_size(btn, MENU_BUTTON_BOX_SIZE, MENU_BUTTON_BOX_SIZE);
        style_menu_button(btn);
        /* 索引 i 即对应 MENU_ICON_TIMER..MENU_ICON_COMPASS，回调中可用于日志 */
        lv_obj_add_event_cb(btn, button_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        /* 容器内居中放白色线性图标，本身不接收点击（事件落到容器） */
        lv_obj_t *icon = lv_image_create(btn);
        lv_image_set_src(icon, menu_icons[i]);
        lv_obj_set_size(icon, MENU_BUTTON_ICON_SIZE, MENU_BUTTON_ICON_SIZE);
        lv_obj_align(icon, LV_ALIGN_CENTER, 0, 0);
        style_icon_white(icon);
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }

    /* 5. 中心关闭按钮 */
    lv_obj_t *close_btn = lv_obj_create(inner_panel);
    lv_obj_set_size(close_btn, MENU_CENTER_BUTTON_BOX_SIZE, MENU_CENTER_BUTTON_BOX_SIZE);
    lv_obj_align(close_btn, LV_ALIGN_CENTER, 0, 0);
    /* 中心按钮：深色圆底，按压变蓝 */
    lv_obj_set_style_radius(close_btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(MENU_COLOR_CENTER_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(close_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(close_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(close_btn, MENU_BUTTON_BOX_PAD, LV_PART_MAIN);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(MENU_COLOR_ACCENT), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(close_btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_clear_flag(close_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(close_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close_btn, close_clicked_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_icon = lv_label_create(close_btn);
    lv_label_set_text(close_icon, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_icon, lv_color_hex(MENU_COLOR_ICON), LV_PART_MAIN);
    lv_obj_align(close_icon, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(close_icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lvgl_port_unlock();
    ESP_LOGI(TAG, "menu shown");
}

void menu_ui_hide(void)
{
    if (!menu_root) {
        return;
    }
    lvgl_port_lock(0);
    /* 删除 overlay 下的所有子节点（dim + menu_root） */
    lv_obj_clean(overlay);
    menu_root = NULL;
    clock_ui_resume();
    lvgl_port_unlock();
    ESP_LOGI(TAG, "menu hidden");
}

/* ---- 内部回调 ------------------------------------------------------------ */

static void long_press_event_cb(lv_event_t *e)
{
    (void)e;
    menu_ui_show();
}

static void button_clicked_cb(lv_event_t *e)
{
    /* 功能按钮当前只保留按压视觉效果，不执行操作且不关闭菜单。 */
    intptr_t idx = (intptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "function button %d clicked (no-op)", (int)idx);
}

static void close_clicked_cb(lv_event_t *e)
{
    (void)e;
    menu_ui_hide();
}

