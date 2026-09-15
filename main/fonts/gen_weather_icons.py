#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 LVGL 天气图标（32x32，透明背景，RGB565A8 格式）。

输出:
    main/fonts/weather_icons.c  — LVGL 图像描述符数组
    main/fonts/weather_icons.h  — 声明 & 枚举

天气类型: 晴 多云 阴 雨 雷雨 雪 雾
"""
import os
import struct
from PIL import Image, ImageDraw

ICON_SIZE = 32
OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))

# 颜色（RGBA）
YELLOW = (255, 193, 7, 255)        # 晴 / 太阳
ORANGE = (255, 152, 0, 255)
GRAY_CLOUD = (158, 158, 158, 255)   # 云
GRAY_DARK = (117, 117, 117, 255)
BLUE_RAIN = (33, 150, 243, 255)     # 雨
BLUE_DARK = (25, 118, 210, 255)
YELLOW_BOLT = (255, 235, 59, 255)   # 闪电
WHITE_SNOW = (255, 255, 255, 255)   # 雪
GRAY_FOG = (189, 189, 189, 255)     # 雾
TRANSPARENT = (0, 0, 0, 0)


def new_canvas():
    return Image.new("RGBA", (ICON_SIZE, ICON_SIZE), TRANSPARENT)


def draw_sun(draw, cx, cy, r, color=YELLOW):
    """绘制带光线的太阳。"""
    draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=color)
    for i in range(8):
        import math
        ang = i * math.pi / 4
        x1 = cx + int(math.cos(ang) * (r + 2))
        y1 = cy + int(math.sin(ang) * (r + 2))
        x2 = cx + int(math.cos(ang) * (r + 5))
        y2 = cy + int(math.sin(ang) * (r + 5))
        draw.line([x1, y1, x2, y2], fill=color, width=2)


def draw_cloud(draw, x, y, w, h, color=GRAY_CLOUD):
    """绘制由三个圆组成的云。"""
    r = h // 2
    draw.ellipse([x, y, x + 2 * r, y + 2 * r], fill=color)
    draw.ellipse([x + r, y - r // 2, x + r + 2 * r, y - r // 2 + 2 * r], fill=color)
    draw.ellipse([x + w - 2 * r, y, x + w, y + 2 * r], fill=color)
    draw.rectangle([x, y + r, x + w, y + 2 * r], fill=color)


# ---- 各天气图标绘制函数 ---------------------------------------------------
def icon_sunny():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    draw_sun(d, 16, 16, 6)
    return img


def icon_cloudy():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    draw_sun(d, 11, 11, 4)
    draw_cloud(d, 10, 14, 16, 10, GRAY_CLOUD)
    return img


def icon_overcast():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    draw_cloud(d, 6, 9, 20, 11, GRAY_DARK)
    draw_cloud(d, 9, 17, 18, 9, GRAY_CLOUD)
    return img


def icon_rainy():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    draw_cloud(d, 6, 6, 20, 10, GRAY_CLOUD)
    for i, x in enumerate([10, 16, 22]):
        d.line([x, 18, x - 2, 24], fill=BLUE_RAIN, width=2)
        d.line([x, 20, x - 2, 26], fill=BLUE_DARK, width=2)
    return img


def icon_thunderstorm():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    draw_cloud(d, 6, 5, 20, 9, GRAY_DARK)
    # 闪电
    d.polygon([(16, 15), (12, 23), (15, 23), (13, 29), (20, 19), (17, 19), (19, 15)],
              fill=YELLOW_BOLT)
    return img


def icon_snowy():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    draw_cloud(d, 6, 6, 20, 9, GRAY_CLOUD)
    for x in [10, 16, 22]:
        # 用线条绘制六角雪花（简化为十字 + 斜线）
        cy = 21
        d.line([x - 3, cy, x + 3, cy], fill=WHITE_SNOW, width=1)
        d.line([x, cy - 3, x, cy + 3], fill=WHITE_SNOW, width=1)
        d.line([x - 2, cy - 2, x + 2, cy + 2], fill=WHITE_SNOW, width=1)
        d.line([x - 2, cy + 2, x + 2, cy - 2], fill=WHITE_SNOW, width=1)
    return img


def icon_foggy():
    img = new_canvas()
    d = ImageDraw.Draw(img)
    for y in [10, 15, 20]:
        d.line([6, y, 26, y], fill=GRAY_FOG, width=3)
    return img


ICONS = {
    "sunny": icon_sunny,
    "cloudy": icon_cloudy,
    "overcast": icon_overcast,
    "rainy": icon_rainy,
    "thunderstorm": icon_thunderstorm,
    "snowy": icon_snowy,
    "foggy": icon_foggy,
}


def rgba_to_rgb565a8(rgba):
    """RGBA -> (rgb565_lo, rgb565_hi, alpha) 三字节。"""
    r, g, b, a = rgba
    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return rgb565 & 0xFF, (rgb565 >> 8) & 0xFF, a


def to_lvgl_c(image, name):
    """将 PIL 图像转为 LVGL RGB565A8 图像描述符的 C 代码片段。

    LVGL 9 RGB565A8 采用平面布局：
      data[0 .. w*h*2-1]   = RGB565 像素（stride = w*2 字节/行）
      data[w*h*2 .. end]   = A8 Alpha 掩码（stride = w 字节/行）
    """
    w, h = image.size
    pixels = image.load()

    rgb_plane = []
    a_plane = []
    for y in range(h):
        for x in range(w):
            lo, hi, a = rgba_to_rgb565a8(pixels[x, y])
            rgb_plane.extend([lo, hi])
            a_plane.append(a)

    flat = rgb_plane + a_plane
    hex_strs = [f"0x{b:02X}" for b in flat]
    lines = []
    for i in range(0, len(hex_strs), 16):
        lines.append("    " + ", ".join(hex_strs[i:i + 16]) + ",")
    data_body = "\n".join(lines)

    return f"""static const uint8_t {name}_map[] = {{
{data_body}
}};

const lv_image_dsc_t {name} = {{
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_RGB565A8,
    .header.flags = 0,
    .header.w = {w},
    .header.h = {h},
    .header.stride = {w * 2},
    .data_size = {w * h * 3},
    .data = {name}_map,
}};
"""


def main():
    c_path = os.path.join(OUTPUT_DIR, "weather_icons.c")
    h_path = os.path.join(OUTPUT_DIR, "weather_icons.h")

    enum_items = []
    c_parts = [
        '#include "lvgl.h"\n',
        '#include "weather_icons.h"\n',
    ]

    for name, fn in ICONS.items():
        img = fn()
        var_name = f"img_weather_{name}"
        enum_items.append(f"  WEATHER_{name.upper()},")
        c_parts.append(to_lvgl_c(img, var_name))

    # 按顺序的查找表
    array_items = ", ".join(f"&img_weather_{n}" for n in ICONS)
    c_parts.append(f"const lv_image_dsc_t *weather_icons[{len(ICONS)}] = {{\n    {array_items}\n}};\n")

    with open(c_path, "w", encoding="utf-8") as f:
        f.write("\n".join(c_parts))

    with open(h_path, "w", encoding="utf-8") as f:
        f.write("""#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
%s
  WEATHER_COUNT,
} weather_type_t;

extern const lv_image_dsc_t *weather_icons[WEATHER_COUNT];

#ifdef __cplusplus
}
#endif
""" % "\n".join(enum_items))

    # 同时导出 PNG 便于人工检查
    png_dir = os.path.join(OUTPUT_DIR, "weather_png")
    os.makedirs(png_dir, exist_ok=True)
    for name, fn in ICONS.items():
        fn().save(os.path.join(png_dir, f"{name}.png"))

    print(f"[OK] 生成 {len(ICONS)} 个天气图标")
    print(f"     C: {c_path}")
    print(f"     H: {h_path}")
    print(f"     PNG 预览: {png_dir}")


if __name__ == "__main__":
    main()
