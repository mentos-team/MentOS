/// @file vscanf.c
/// @brief Reading formatting routines.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "ctype.h"
#include "fs/vfs.h"
#include "io/debug.h"
#include "stdio.h"
#include "string.h"

/// @brief Read formatted data from string.
/// @param str String processed as source to retrieve the data.
/// @param s  Format string, following the same specifications as printf.
/// @param ap The list of arguments where the values are stored.
/// @return On success, the function returns the number of items of the
///         argument list successfully filled. EOF otherwise.
static int vsscanf(const char *str, const char *s, va_list ap)
{
    int count    = 0;
    int noassign = 0;
    int width    = 0;
    int base     = 0;
    const char *tc;
    char tmp[BUFSIZ];

    while (*s && *str) {
        while (isspace(*s)) {
            ++s;
        }
        if (*s == '%') {
            ++s;
            for (; *s; ++s) {
                if (strchr("dibouxcsefg%", *s)) {
                    break;
                }
                if (*s == '*') {
                    {
                        noassign = 1;
                    }
                } else if (isdigit(*s)) {
                    for (tc = s; isdigit(*s); ++s) {
                        {
                            ;
                        }
                    }
                    strncpy(tmp, tc, s - tc);
                    tmp[s - tc] = '\0';
                    width       = strtol(tmp, NULL, 10);
                    --s;
                }
            }
            if (*s == 's') {
                while (isspace(*str)) {
                    ++str;
                }
                if (!width) {
                    width = strcspn(str, " \t\n\r\f\v");
                }
                if (!noassign) {
                    char *string = va_arg(ap, char *);
                    strncpy(string, str, width);
                    string[width] = '\0';
                }
                str += width;
            } else if (*s == 'c') {
                while (isspace(*str)) {
                    ++str;
                }
                if (!width) {
                    width = 1;
                }
                if (!noassign) {
                    strncpy(va_arg(ap, char *), str, width);
                }
                str += width;
            } else if (strchr("duxob", *s)) {
                while (isspace(*str)) {
                    ++str;
                }
                if (*s == 'd' || *s == 'u') {
                    base = 10;
                } else if (*s == 'x') {
                    base = 16;
                } else if (*s == 'o') {
                    base = 8;
                } else if (*s == 'b') {
                    base = 2;
                }
                if (!width) {
                    if (isspace(*(s + 1)) || *(s + 1) == 0) {
                        width = strcspn(str, " \t\n\r\f\v");
                    } else {
                        width = strchr(str, *(s + 1)) - str;
                    }
                }
                strncpy(tmp, str, width);
                tmp[width] = '\0';
                str += width;
                if (!noassign) {
                    *va_arg(ap, unsigned int *) = strtol(tmp, NULL, base);
                }
            }
            if (!noassign) {
                ++count;
            }
            width = noassign = 0;
            ++s;
        } else {
            while (isspace(*str)) {
                ++str;
            }
            if (*s != *str) {
                break;
            }
            ++s, ++str;
        }
    }
    return (count);
}

int sscanf(const char *str, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    int count = vsscanf(str, format, ap);
    va_end(ap);
    return count;
}
