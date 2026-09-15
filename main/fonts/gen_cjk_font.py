#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
为 LVGL 时钟表盘生成定制 CJK 字体 (size=18, bpp=4)。
字符集覆盖：二十四节气、天干地支、农历日期、天气、星期/时间 UI 常用字。

用法:
    python main/fonts/gen_cjk_font.py

依赖:
    Node.js + lv_font_conv (npx lv_font_conv)
    字体源: managed_components/lvgl__lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf
"""
import os
import shutil
import subprocess
import sys

# 项目根目录
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT_ROOT = os.path.dirname(ROOT)

FONT_SOURCE = os.path.join(
    PROJECT_ROOT,
    "managed_components",
    "lvgl__lvgl",
    "scripts",
    "built_in_font",
    "SourceHanSansSC-Normal.otf",
)
OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "lv_font_cjk_clock_18.c")

# ---- 字符集定义 -------------------------------------------------------------
# 二十四节气
SOLAR_TERMS = "立春雨水惊蛰春分清明谷雨立夏小满芒种夏至小暑大暑立秋处暑白露秋分寒露霜降立冬小雪大雪冬至小寒大寒"

# 天干地支
HEAVENLY_STEMS = "甲乙丙丁戊己庚辛壬癸"
EARTHLY_BRANCHES = "子丑寅卯辰巳午未申酉戌亥"

# 农历日期：月份与日序
LUNAR_MONTHS = "正二三四五六七八九十冬腊闰"          # 正月..冬月腊月, 闰
LUNAR_DAYS = "初廿卅"                                   # 初X, 廿X, 卅X
LUNAR_DAY_NUMS = "一二三四五六七八九十"                 # 初一..初十, 十一..二十, 廿一..廿九, 三十

# 天气
WEATHER = "晴朗多云阴雨阵雷电风雪雹霰雾露霜凇霾沙尘台风暴飓狂轻微和寒凉爽暖湿干燥温热冷"

# 温度 / 风力
TEMP_WIND = "温度度级风力"

# 星期 / 日期 / 时间 UI
WEEKDAY = "星期周日一二三四五六天"
DATETIME = "月份日年时分秒上午下午今明清前后节气气候物候季节春夏秋冬四季农历公历阳历阴历节日假日"

# 基本数字与符号，后面加个空格，避免字体生成时被 lv_font_conv 过滤掉
BASIC = "0123456789°C/-.· "

# 合并并去重
ALL_CHARS = (
    SOLAR_TERMS
    + HEAVENLY_STEMS
    + EARTHLY_BRANCHES
    + LUNAR_MONTHS
    + LUNAR_DAYS
    + LUNAR_DAY_NUMS
    + WEATHER
    + TEMP_WIND
    + WEEKDAY
    + DATETIME
    + BASIC
)
# 去重并按 Unicode 排序
CODEPOINTS = sorted({ord(ch) for ch in ALL_CHARS})


def to_ranges(codepoints):
    """将有序码点列表合并为连续区间，返回 [(start, end), ...]。"""
    ranges = []
    start = prev = codepoints[0]
    for cp in codepoints[1:]:
        if cp == prev + 1:
            prev = cp
            continue
        ranges.append((start, prev))
        start = prev = cp
    ranges.append((start, prev))
    return ranges


RANGES = to_ranges(CODEPOINTS)


def main() -> int:
    if not os.path.isfile(FONT_SOURCE):
        print(f"[ERR] 字体源不存在: {FONT_SOURCE}", file=sys.stderr)
        return 1

    print(f"[INFO] 字体源: {FONT_SOURCE}")
    print(f"[INFO] 字符数: {len(CODEPOINTS)} (去重后), 区间数: {len(RANGES)}")

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Windows 上 npx 为 npx.cmd
    npx = shutil.which("npx") or shutil.which("npx.cmd") or "npx"
    cmd = [
        npx, "lv_font_conv",
        "--font", FONT_SOURCE,
        "--size", "18",
        "--bpp", "4",
        "--format", "lvgl",
        "--no-compress",
        "--no-prefilter",
        "--output", OUTPUT_FILE,
    ]
    # 用 --range 传码点区间，避免 shell 解析 Unicode
    for start, end in RANGES:
        if start == end:
            cmd += ["--range", f"0x{start:04X}"]
        else:
            cmd += ["--range", f"0x{start:04X}-0x{end:04X}"]

    print(f"[INFO] 运行 lv_font_conv ({len(cmd) - 3} 个参数) ...")
    result = subprocess.run(cmd, capture_output=True, text=True, shell=(os.name == "nt"))
    if result.returncode != 0:
        print("[ERR] lv_font_conv 失败:", file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        return result.returncode

    size_kb = os.path.getsize(OUTPUT_FILE) / 1024
    print(f"[OK] 已生成: {OUTPUT_FILE}  ({size_kb:.1f} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
