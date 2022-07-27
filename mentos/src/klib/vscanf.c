/// @file vscanf.c
/// @brief Reading formatting routines.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fs/vfs.h"
#include "ctype.h"
#include "string.h"
#include "io/debug.h"
#include "stdio.h"

static int vsscanf(const char *buf, const char *s, va_list ap)
{
    int count = 0, noassign = 0, width = 0, base = 0;
    const char *tc;
    char tmp[BUFSIZ];

    while (*s && *buf) {
        while (isspace(*s))
            ++s;
        if (*s == '%') {
            ++s;
            for (; *s; ++s) {
                if (strchr("dibouxcsefg%", *s))
                    break;
                if (*s == '*')
                    noassign = 1;
                else if (isdigit(*s)) {
                    for (tc = s; isdigit(*s); ++s)
                        ;
                    strncpy(tmp, tc, s - tc);
                    tmp[s - tc] = '\0';
                    width       = strtol(tmp, NULL, 10);
                    --s;
                }
            }
            if (*s == 's') {
                while (isspace(*buf))
                    ++buf;
                if (!width)
                    width = strcspn(buf, " \t\n\r\f\v");
                if (!noassign) {
                    char *string = va_arg(ap, char *);
                    strncpy(string, buf, width);
                    string[width] = '\0';
                }
                buf += width;
            } else if (*s == 'c') {
                while (isspace(*buf))
                    ++buf;
                if (!width)
                    width = 1;
                if (!noassign) {
                    strncpy(va_arg(ap, char *), buf, width);
                }
                buf += width;
            } else if (strchr("duxob", *s)) {
                while (isspace(*buf))
                    ++buf;
                if (*s == 'd' || *s == 'u')
                    base = 10;
                else if (*s == 'x')
                    base = 16;
                else if (*s == 'o')
                    base = 8;
                else if (*s == 'b')
                    base = 2;
                if (!width) {
                    if (isspace(*(s + 1)) || *(s + 1) == 0)
                        width = strcspn(buf, " \t\n\r\f\v");
                    else
                        width = strchr(buf, *(s + 1)) - buf;
                }
                strncpy(tmp, buf, width);
                tmp[width] = '\0';
                buf += width;
                if (!noassign)
                    *va_arg(ap, unsigned int *) = strtol(tmp, NULL, base);
            }
            if (!noassign)
                ++count;
            width = noassign = 0;
            ++s;
        } else {
            while (isspace(*buf))
                ++buf;
            if (*s != *buf)
                break;
            else
                ++s, ++buf;
        }
    }
    return (count);
}

int sscanf(const char *buf, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    int count = vsscanf(buf, fmt, ap);
    va_end(ap);
    return count;
}
