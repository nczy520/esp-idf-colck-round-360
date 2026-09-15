#pragma once

#include <stdbool.h>
#include <time.h>

typedef struct {
    int year;
    int month;
    int day;
    bool leap_month;
} lunar_date_t;

typedef struct {
    lunar_date_t lunar;
    char lunar_month_name[8];
    char lunar_day_name[8];
    const char *solar_term;
    const char *festival;
    char year_ganzhi[16];
    char month_ganzhi[16];
    char day_ganzhi[16];
} calendar_info_t;

bool calendar_get_info(const struct tm *solar, calendar_info_t *info);