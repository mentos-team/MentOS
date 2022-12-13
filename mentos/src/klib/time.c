/// @file time.c
/// @brief Clock functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "io/debug.h"
#include "time.h"
#include "stdio.h"
#include "stddef.h"
#include "io/port_io.h"
#include "hardware/timer.h"
#include "drivers/rtc.h"

static const char *str_weekdays[] = { "Sunday", "Monday", "Tuesday", "Wednesday",
                                      "Thursday", "Friday", "Saturday" };

static const char *str_months[] = { "January", "February", "March", "April",
                                    "May", "June", "July", "August",
                                    "September", "October", "November", "December" };

time_t sys_time(time_t *time)
{
    tm_t curr_time;
    gettime(&curr_time);
    // January and February are counted as months 13 and 14 of the previous year.
    if (curr_time.tm_mon <= 2) {
        curr_time.tm_mon += 12;
        curr_time.tm_year -= 1;
    }
    time_t t;
    // Convert years to days
    t = (365 * curr_time.tm_year) + (curr_time.tm_year / 4) - (curr_time.tm_year / 100) +
        (curr_time.tm_year / 400);
    // Convert months to days
    t += (30 * curr_time.tm_mon) + (3 * (curr_time.tm_mon + 1) / 5) + curr_time.tm_mday;
    // Unix time starts on January 1st, 1970
    t -= 719561;
    // Convert days to seconds
    t *= 86400;
    // Add hours, minutes and seconds
    t += (3600 * curr_time.tm_hour) + (60 * curr_time.tm_min) + curr_time.tm_sec;
    if (time) {
        (*time) = t;
    }
    return t;
}

time_t difftime(time_t time1, time_t time2)
{
    return time1 - time2;
}

/// @brief Computes day of week
/// @param y Year
/// @param m Month of year (in range 1 to 12)
/// @param d Day of month (in range 1 to 31)
/// @return Day of week (in range 1 to 7)
static inline int day_of_week(unsigned int y, unsigned int m, unsigned int d)
{
    int h, j, k;
    // January and February are counted as months 13 and 14 of the previous year
    if (m <= 2) {
        m += 12;
        y -= 1;
    }
    // J is the century
    j = (int)(y / 100);
    // K the year of the century
    k = (int)(y % 100);
    // Compute H using Zeller's congruence
    h = (int)(d + (26 * (m + 1) / 10) + k + (k / 4) + (5 * j) + (j / 4));
    // Return the day of the week
    return ((h + 5) % 7) + 1;
}

tm_t *localtime(const time_t *time)
{
    static tm_t date;
    unsigned int a, b, c, d, e, f;
    time_t t = *time;
    // Negative Unix time values are not supported
    if (t < 1) {
        t = 0;
    }
    //Retrieve hours, minutes and seconds
    date.tm_sec = (int)(t % 60);
    t /= 60;
    date.tm_min = (int)(t % 60);
    t /= 60;
    date.tm_hour = (int)(t % 24);
    t /= 24;
    // Convert Unix time to date
    a = (unsigned int)((4 * t + 102032) / 146097 + 15);
    b = (unsigned int)(t + 2442113 + a - (a / 4));
    c = (20 * b - 2442) / 7305;
    d = b - 365 * c - (c / 4);
    e = d * 1000 / 30601;
    f = d - e * 30 - e * 601 / 1000;
    // January and February are counted as months 13 and 14 of the previous year
    if (e <= 13) {
        c -= 4716;
        e -= 1;
    } else {
        c -= 4715;
        e -= 13;
    }
    //Retrieve year, month and day
    date.tm_year = (int)c;
    date.tm_mon  = (int)e;
    date.tm_mday = (int)f;
    // Calculate day of week.
    date.tm_wday = day_of_week(date.tm_year, date.tm_mon, date.tm_mday);
    return &date;
}
