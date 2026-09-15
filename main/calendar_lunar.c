#include "calendar_lunar.h"

#include <stdio.h>

static const unsigned int lunar_info[] = {
    0x04bd8,0x04ae0,0x0a570,0x054d5,0x0d260,0x0d950,0x16554,0x056a0,0x09ad0,0x055d2,    //1900-1909
    0x04ae0,0x0a5b6,0x0a4d0,0x0d250,0x1d255,0x0b540,0x0d6a0,0x0ada2,0x095b0,0x14977,    //1910-1919
    0x04970,0x0a4b0,0x0b4b5,0x06a50,0x06d40,0x1ab54,0x02b60,0x09570,0x052f2,0x04970,    //1920-1929
    0x06566,0x0d4a0,0x0ea50,0x06e95,0x05ad0,0x02b60,0x186e3,0x092e0,0x1c8d7,0x0c950,    //1930-1939
    0x0d4a0,0x1d8a6,0x0b550,0x056a0,0x1a5b4,0x025d0,0x092d0,0x0d2b2,0x0a950,0x0b557,    //1940-1949
    0x06ca0,0x0b550,0x15355,0x04da0,0x0a5d0,0x14573,0x052d0,0x0a9a8,0x0e950,0x06aa0,    //1950-1959
    0x0aea6,0x0ab50,0x04b60,0x0aae4,0x0a570,0x05260,0x0f263,0x0d950,0x05b57,0x056a0,    //1960-1969
    0x096d0,0x04dd5,0x04ad0,0x0a4d0,0x0d4d4,0x0d250,0x0d558,0x0b540,0x0b5a0,0x195a6,    //1970-1979
    0x095b0,0x049b0,0x0a974,0x0a4b0,0x0b27a,0x06a50,0x06d40,0x0af46,0x0ab60,0x09570,    //1980-1989
    0x04af5,0x04970,0x064b0,0x074a3,0x0ea50,0x06b58,0x05ac0,0x0ab60,0x096d5,0x092e0,    //1990-1999
    0x0c960,0x0d954,0x0d4a0,0x0da50,0x07552,0x056a0,0x0abb7,0x025d0,0x092d0,0x0cab5,    //2000-2009
    0x0a950,0x0b4a0,0x0baa4,0x0ad50,0x055d9,0x04ba0,0x0a5b0,0x15176,0x052b0,0x0a930,    //2010-2019
    0x07954,0x06aa0,0x0ad50,0x05b52,0x04b60,0x0a6e6,0x0a4e0,0x0d260,0x0ea65,0x0d530,    //2020-2029
    0x05aa0,0x076a3,0x096d0,0x04bd7,0x04ad0,0x0a4d0,0x1d0b6,0x0d250,0x0d520,0x0dd45,    //2030-2039
    0x0b5a0,0x056d0,0x055b2,0x049b0,0x0a577,0x0a4b0,0x0aa50,0x1b255,0x06d20,0x0ada0,    //2040-2049
    0x14b63,0x09370,0x049f8,0x04970,0x064b0,0x168a6,0x0ea50,0x06b20,0x1a6c4,0x0aae0,    //2050-2059
    0x092e0,0x0d2e3,0x0c960,0x0d550,0x0d559,0x0d4a0,0x0da50,0x05d55,0x056a0,0x0a6d0,
    0x055d4,0x052d0,0x0a9b8,0x0a950,0x0b4a0,0x0b6a6,0x0ad50,0x055a0,0x0aba4,0x04b60,
    0x0a5b0,0x151b6,0x052b0,0x0a930,0x07954,0x06aa0,0x0ad50,0x05b52,0x04b60,0x0a6e6,
    0x0a4e0,0x0d260,0x0ea65,0x0d530,0x05aa0,0x076a3,0x096d0,0x04bd7,0x04ad0,0x0a4d0,
    0x1d0b6,0x0d250,0x0d520,0x0dd45,0x0b5a0,0x056d0,0x055b2,0x049b0,0x0a577,0x0a4b0,
    0x0aa50,0x1b255,0x06d20,0x0ada0,0x14b63,0x09370,0x049f8,0x04970,0x064b0,0x168a6,
    0x0ea50
};
static const char *const stems[] = {"甲","乙","丙","丁","戊","己","庚","辛","壬","癸"};
static const char *const branches[] = {"子","丑","寅","卯","辰","巳","午","未","申","酉","戌","亥"};
static const char *const month_names[] = {"正","二","三","四","五","六","七","八","九","十","冬","腊"};
static const char *const day_names[] = {"","初一","初二","初三","初四","初五","初六","初七","初八","初九","初十","十一","十二","十三","十四","十五","十六","十七","十八","十九","二十","廿一","廿二","廿三","廿四","廿五","廿六","廿七","廿八","廿九","三十"};
static const char *const terms[] = {"小寒","大寒","立春","雨水","惊蛰","春分","清明","谷雨","立夏","小满","芒种","夏至","小暑","大暑","立秋","处暑","白露","秋分","寒露","霜降","立冬","小雪","大雪","冬至"};
static const int term_minutes[] = {0,21208,42467,63836,85337,107014,128867,150921,173149,195551,218072,240693,263343,285989,308563,331033,353350,375494,397447,419210,440795,462224,483532,504758};

static int leap_month(int year) { return lunar_info[year - 1900] & 15; }
static int month_days(int year, int month) { return (lunar_info[year - 1900] & (0x10000 >> month)) ? 30 : 29; }
static int leap_days(int year) { return leap_month(year) ? ((lunar_info[year - 1900] & 0x10000) ? 30 : 29) : 0; }
static int year_days(int year) { int days = 348; for (unsigned int bit = 0x8000; bit > 8; bit >>= 1) days += (lunar_info[year - 1900] & bit) ? 1 : 0; return days + leap_days(year); }

static bool solar_to_lunar(const struct tm *solar, lunar_date_t *result)
{
    int year = solar->tm_year + 1900;
    if (year < 1900 || year > 2100) return false;
    struct tm base = {0}; base.tm_year = 0; base.tm_mon = 0; base.tm_mday = 31;
    struct tm target = *solar; target.tm_hour = 12; target.tm_min = 0; target.tm_sec = 0;
    int offset = (int)((mktime(&target) - mktime(&base)) / 86400);
    int lunar_year = 1900;
    while (offset >= year_days(lunar_year)) offset -= year_days(lunar_year++);
    int lunar_month = 1; int leap = leap_month(lunar_year); bool is_leap = false;
    while (lunar_month <= 12) {
        int days = is_leap ? leap_days(lunar_year) : month_days(lunar_year, lunar_month);
        if (offset < days) break;
        offset -= days;
        if (leap == lunar_month && !is_leap) is_leap = true;
        else { if (is_leap) is_leap = false; lunar_month++; }
    }
    result->year = lunar_year; result->month = lunar_month; result->day = offset + 1; result->leap_month = is_leap;
    return true;
}

static const char *festival(const lunar_date_t *date)
{
    if (date->month == 1 && date->day == 1) return "春节";
    if (date->month == 1 && date->day == 15) return "元宵";
    if (date->month == 5 && date->day == 5) return "端午";
    if (date->month == 7 && date->day == 7) return "七夕";
    if (date->month == 8 && date->day == 15) return "中秋";
    if (date->month == 9 && date->day == 9) return "重阳";
    if (date->month == 12 && date->day == 8) return "腊八";
    if (date->month == 12 && date->day == 24) return "小年";
    return "";
}

static const char *solar_term(const struct tm *solar)
{
    static char name[16];
    int year = solar->tm_year + 1900;
    for (int i = 0; i < 24; i++) {
        struct tm base = {0}; base.tm_year = 0; base.tm_mon = 0; base.tm_mday = 6; base.tm_hour = 2; base.tm_min = 5;
        time_t term = mktime(&base) + (time_t)((31556925974.7 * (year - 1900) + term_minutes[i] * 60000.0) / 1000.0);
        struct tm date; localtime_r(&term, &date);
        if (date.tm_mon == solar->tm_mon && date.tm_mday == solar->tm_mday) { snprintf(name, sizeof(name), "%s", terms[i]); return name; }
    }
    return "";
}

bool calendar_get_info(const struct tm *solar, calendar_info_t *info)
{
    if (!solar || !info || !solar_to_lunar(solar, &info->lunar)) return false;
    snprintf(info->lunar_month_name, sizeof(info->lunar_month_name), "%s月", month_names[info->lunar.month - 1]);
    snprintf(info->lunar_day_name, sizeof(info->lunar_day_name), "%s", day_names[info->lunar.day]);
    info->solar_term = solar_term(solar); info->festival = festival(&info->lunar);
    int year_index = (info->lunar.year - 4) % 60; if (year_index < 0) year_index += 60;
    snprintf(info->year_ganzhi, sizeof(info->year_ganzhi), "%s%s", stems[year_index % 10], branches[year_index % 12]);
    int month_index = ((year_index % 5) * 2 + info->lunar.month + 1) % 10;
    snprintf(info->month_ganzhi, sizeof(info->month_ganzhi), "%s%s", stems[month_index], branches[(info->lunar.month + 1) % 12]);
    int day_index = (367 * (solar->tm_year + 1900) - (7 * ((solar->tm_year + 1900) + ((solar->tm_mon + 9) / 12))) / 4 + (275 * (solar->tm_mon + 1)) / 9 + solar->tm_mday + 1729777) % 60;
    snprintf(info->day_ganzhi, sizeof(info->day_ganzhi), "%s%s", stems[day_index % 10], branches[day_index % 12]);
    return true;
}