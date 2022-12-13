/// @file time.c
/// @brief Clock functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "time.h"
#include "system/syscall_types.h"
#include "sys/errno.h"
#include "string.h"
#include "stdio.h"

static const char *weekdays[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const char *months[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

/// @brief Time function.
_syscall1(time_t, time, time_t *, t)

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

size_t strftime(char *str, size_t maxsize, const char *format, const tm_t *timeptr)
{
    if (format == NULL || str == NULL || maxsize <= 0) {
        return 0;
    }
    char *f = (char *)format;
    char *s = str;
    int value;
    size_t ret = 0;
    while ((*f) != '\0') {
        if ((*f) != '%') {
            (*s++) = (*f);
        } else {
            ++f;
            switch (*f) {
#if 0
                case 'a':    /* locale's abbreviated weekday name */
                    PUT_STRN(weekdays[timeptr->tm_wday], 3);
                    break;

                case 'A':    /* locale's full weekday name */
                    PUT_STR(weekdays[timeptr->tm_wday]);
                    break;
#endif
            case 'b': // Abbreviated month name
            {
                strncat(s, months[timeptr->tm_mon], 3);
                break;
            }
            case 'B': // Full month name
            {
                unsigned int i = (unsigned int)timeptr->tm_mon;
                strcat(s, months[i]);
                break;
            }
            case 'd': /* day of month as decimal number (01-31) */
            {
                value  = timeptr->tm_mday;
                (*s++) = (char)('0' + ((value / 10) % 10));
                (*s++) = (char)('0' + (value % 10));
                break;
            }
            case 'H': /* hour (24-hour clock) as decimal (00-23) */
            {
                value  = timeptr->tm_hour;
                (*s++) = (char)('0' + ((value / 10) % 10));
                (*s++) = (char)('0' + (value % 10));
                break;
            }
#if 0
                case 'I':    /* hour (12-hour clock) as decimal (01-12) */
                    SPRINTF("%02d", ((timeptr->tm_hour + 11) % 12) + 1);
                    break;
#endif
            case 'j': // Day of year as decimal number (001-366)
            {
                value  = timeptr->tm_yday;
                (*s++) = (char)('0' + ((value / 10) % 10));
                (*s++) = (char)('0' + (value % 10));
                break;
            }
            case 'm': // Month as decimal number (01-12)
            {
                value  = timeptr->tm_mon;
                (*s++) = (char)('0' + ((value / 10) % 10));
                (*s++) = (char)('0' + (value % 10));
                break;
            }
#if 0
                case 'M':    /* month as decimal number (00-59) */
                    SPRINTF("%02d", timeptr->tm_min);
                    break;

                case 'p':    /* AM/PM designations (12-hour clock) */
                    PUT_STR(timeptr->tm_hour < 12 ? "AM" : "PM");
                    /* s.b. locale-specific */
                    break;

                case 'S':    /* second as decimal number (00-61) */
                    SPRINTF("%02d", timeptr->tm_sec);
                    break;

                case 'U':    /* week of year (first Sunday = 1) (00-53) */
                case 'W':    /* week of year (first Monday = 1) (00-53) */
                {
                    int fudge = timeptr->tm_wday - timeptr->tm_yday % 7;
                    if (*fp == 'W') {
                        fudge--;
                    }
                    fudge = (fudge + 7) % 7;      /* +7 so not negative */
                    SPRINTF("%02d", (timeptr->tm_yday + fudge) / 7);
                    break;
                }

                case 'w':    /* weekday (0-6), Sunday is 0 */
                    SPRINTF("%02d", timeptr->tm_wday);
                    break;

                case 'x':    /* appropriate date representation */
                    RECUR("%B %d, %Y");    /* s.b. locale-specific */
                    break;

                case 'X':    /* appropriate time representation */
                    RECUR("%H:%M:%S");    /* s.b. locale-specific */
                    break;

                case 'y':    /* year without century as decimal (00-99) */
                    SPRINTF("%02d", timeptr->tm_year % 100);
                    break;

                case 'Y':    /* year with century as decimal */
                    SPRINTF("%d", timeptr->tm_year + 1900);
                    break;

                case 'Z':    /* time zone name or abbreviation */
                    /* XXX %%% */
                    break;

                case '%':
                    PUT_CH('%');
                    break;
#endif
            default:
                (*s++) = '%';
                (*s++) = (*f);
            }
        }
        ++f;
    }

#if 0
#define PUT_CH(c) (ret < maxsize ? (*dp++ = (c), ret++) : 0)
#define PUT_STR(str)   \
    p = (char *)(str); \
    goto dostr;
#define PUT_STRN(str, n) \
    p   = (char *)(str); \
    len = (n);           \
    goto dostrn;
#define SPRINTF(fmt, arg)      \
    sprintf(tmpbuf, fmt, arg); \
    p = tmpbuf;                \
    goto dostr;
#define RECUR(fmt)                                            \
    {                                                         \
        size_t r = strftime(dp, maxsize - ret, fmt, timeptr); \
        if (r <= 0)                                           \
            return r;                                         \
        dp += r;                                              \
        ret += r;                                             \
    }

    char* fp = (char*) format;
    char* dp = s;
    size_t ret = 0;

    if (format == NULL || s == NULL || maxsize <= 0) {
        return 0;
    }

    while (*fp != '\0') {
        char* p;
        int len;
        char tmpbuf[10];

        if (*fp != '%') {
            PUT_CH(*fp++);
            continue;
        }

        fp++;

        switch (*fp) {

        case 'a':    /* locale's abbreviated weekday name */
        PUT_STRN(weekdays[timeptr->tm_wday], 3);
            break;

        case 'A':    /* locale's full weekday name */
        PUT_STR(weekdays[timeptr->tm_wday]);
            break;

        case 'b':    /* locale's abbreviated month name */
        PUT_STRN(months[timeptr->tm_mon], 3);
            break;

        case 'B':    /* locale's full month name */
        PUT_STR(months[timeptr->tm_mon]);
            break;

        case 'c':    /* appropriate date and time representation */
        RECUR("%X %x");        /* s.b. locale-specific */
            break;

        case 'd':    /* day of month as decimal number (01-31) */
        SPRINTF("%02d", timeptr->tm_mday);
            break;

        case 'H':    /* hour (24-hour clock) as decimal (00-23) */
        SPRINTF("%02d", timeptr->tm_hour);
            break;

        case 'I':    /* hour (12-hour clock) as decimal (01-12) */
        SPRINTF("%02d", ((timeptr->tm_hour + 11) % 12) + 1);
            break;

        case 'j':    /* day of year as decimal number (001-366) */
        SPRINTF("%03d", timeptr->tm_yday + 1);
            break;

        case 'm':    /* month as decimal number (01-12) */
        SPRINTF("%02d", timeptr->tm_mon + 1);
            break;

        case 'M':    /* month as decimal number (00-59) */
        SPRINTF("%02d", timeptr->tm_min);
            break;

        case 'p':    /* AM/PM designations (12-hour clock) */
        PUT_STR(timeptr->tm_hour < 12 ? "AM" : "PM");
            /* s.b. locale-specific */
            break;

        case 'S':    /* second as decimal number (00-61) */
        SPRINTF("%02d", timeptr->tm_sec);
            break;

        case 'U':    /* week of year (first Sunday = 1) (00-53) */
        case 'W':    /* week of year (first Monday = 1) (00-53) */
        {
            int fudge = timeptr->tm_wday - timeptr->tm_yday % 7;
            if (*fp == 'W') {
                fudge--;
            }
            fudge = (fudge + 7) % 7;      /* +7 so not negative */
            SPRINTF("%02d", (timeptr->tm_yday + fudge) / 7);
            break;
        }

        case 'w':    /* weekday (0-6), Sunday is 0 */
        SPRINTF("%02d", timeptr->tm_wday);
            break;

        case 'x':    /* appropriate date representation */
        RECUR("%B %d, %Y");    /* s.b. locale-specific */
            break;

        case 'X':    /* appropriate time representation */
        RECUR("%H:%M:%S");    /* s.b. locale-specific */
            break;

        case 'y':    /* year without century as decimal (00-99) */
        SPRINTF("%02d", timeptr->tm_year % 100);
            break;

        case 'Y':    /* year with century as decimal */
        SPRINTF("%d", timeptr->tm_year + 1900);
            break;

        case 'Z':    /* time zone name or abbreviation */
            /* XXX %%% */
            break;

        case '%':
            PUT_CH('%');
            break;

        dostr:
            while (*p != '\0')
                PUT_CH(*p++);
            break;

        dostrn:
            while (len-- > 0)
                PUT_CH(*p++);
            break;
        }

        ++fp;
    }
    if (ret >= maxsize) {
        s[maxsize - 1] = '\0';
        return 0;
    }
    *dp = '\0';
#endif
    return ret;
}

/// @brief nanosleep function.
_syscall2(int, nanosleep, const timespec *, req, timespec *, rem)

unsigned int sleep(unsigned int seconds)
{
    timespec req, rem;
    req.tv_sec = seconds;
    // Call the nanosleep.
    int __ret = nanosleep(&req, &rem);
    // If the call to the nanosleep is interrupted by a signal handler,
    // then it returns -1 with errno set to EINTR.
    if ((__ret == -1) && (errno == EINTR)) {
        return rem.tv_sec;
    }
    return 0;
}

_syscall2(int, getitimer, int, which, itimerval *, curr_value)

_syscall3(int, setitimer, int, which, const itimerval *, new_value, itimerval *, old_value)
