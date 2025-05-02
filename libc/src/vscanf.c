/// @file vscanf.c
/// @brief Reading formatting routines.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
/// @brief Read formatted data from a string buffer.
/// @param str The input string to parse.
/// @param s The format string, like in printf/scanf.
/// @param ap The list of output arguments.
/// @return Number of successful assignments, or EOF (-1) on failure.
static int __vsscanf(const char *str, const char *s, va_list ap)
{
    int count    = 0; // Successful conversions
    int noassign = 0; // If '*' is used to skip assignment
    int width    = 0; // Max characters to process
    int base     = 0; // Numeric base
    const char *tc;
    char tmp[BUFSIZ];

    while (*s && *str) {
        // Match format whitespace to input whitespace (POSIX rule)
        if (isspace(*s)) {
            while (isspace(*s))
                ++s;
            while (isspace(*str))
                ++str;
            continue;
        }

        if (*s == '%') {
            ++s;

            // Parse optional assignment suppression and width
            for (; *s; ++s) {
                if (strchr("dibouxcs", *s))
                    break; // supported specifiers
                if (*s == '*') {
                    noassign = 1;
                } else if (isdigit(*s)) {
                    for (tc = s; isdigit(*s); ++s)
                        ;
                    strncpy(tmp, tc, s - tc);
                    tmp[s - tc] = '\0';
                    width       = strtol(tmp, NULL, 10);
                    --s; // step back so loop resumes at specifier
                }
            }

            // Handle string specifier: %s
            if (*s == 's') {
                while (isspace(*str))
                    ++str;
                if (!width) {
                    width = strcspn(str, " \t\n\r\f\v");
                }
                if (!noassign) {
                    char *string = va_arg(ap, char *);
                    strncpy(string, str, width);
                    string[width] = '\0';
                }
                str += width;
            }

            // Handle character specifier: %c
            else if (*s == 'c') {
                // Note: unlike other specifiers, %c does NOT skip whitespace
                if (!width)
                    width = 1;
                if (!noassign) {
                    char *dst = va_arg(ap, char *);
                    strncpy(dst, str, width);
                }
                str += width;
            }

            // Handle numeric specifiers: %d %u %x %o %b
            else if (strchr("duxob", *s)) {
                while (isspace(*str))
                    ++str;

                // Determine base
                switch (*s) {
                case 'd':
                case 'u':
                    base = 10;
                    break;
                case 'x':
                    base = 16;
                    break;
                case 'o':
                    base = 8;
                    break;
                case 'b':
                    base = 2;
                    break;
                }

                // Calculate width if unspecified
                if (!width) {
                    char next = *(s + 1);
                    if (isspace(next) || next == '\0') {
                        width = strcspn(str, " \t\n\r\f\v");
                    } else {
                        char *next_sep = strchr(str, next);
                        width          = next_sep ? (next_sep - str) : strcspn(str, " \t\n\r\f\v");
                    }
                }

                strncpy(tmp, str, width);
                tmp[width] = '\0';
                str += width;

                if (!noassign) {
                    *va_arg(ap, unsigned int *) = strtol(tmp, NULL, base);
                }
            }

            // Increment successful assignment count
            if (!noassign)
                ++count;
            // Reset flags for next format
            width = noassign = 0;
            ++s;
        }

        // Match literal characters (outside of '%')
        else {
            while (isspace(*str))
                ++str;
            if (*s != *str)
                break;
            ++s;
            ++str;
        }
    }
    return count;
}

/// @brief Read formatted data from file.
/// @param fd the file descriptor associated with the file.
/// @param format format string, following the same specifications as printf.
/// @param ap the list of arguments where the values are stored.
/// @param blocking if 0, the function will return immediately if no data is
/// @return On success, the function returns the number of items of the
///         argument list successfully filled. EOF otherwise.
static int __vfscanf(int fd, const char *format, va_list ap, int blocking)
{
    if (fd < 0 || format == NULL) {
        return -1;
    }
    char str[BUFSIZ + 1] = {0};
    ssize_t idx          = 0;
    while (idx < BUFSIZ) {
        char c;
        ssize_t res = read(fd, &c, 1);
        if (res == 0) {
            // Wait for data.
            if (blocking) {
                continue;
            }
            // Return what we have so far.
            break;
        }
        if (res < 0) {
            return -1;
        }
        if (c == '\n') {
            break;
        }
        str[idx++] = c;
    }
    str[idx] = '\0';
    return __vsscanf(str, format, ap);
}

int scanf(const char *format, ...)
{
    int count;
    va_list ap;
    va_start(ap, format);
    count = __vfscanf(STDIN_FILENO, format, ap, 1);
    va_end(ap);
    return (count);
}

int fscanf(int fd, const char *format, ...)
{
    int count;
    va_list ap;

    va_start(ap, format);
    count = __vfscanf(fd, format, ap, 0);
    va_end(ap);
    return (count);
}

int sscanf(const char *str, const char *format, ...)
{
    int count;
    va_list ap;

    va_start(ap, format);
    count = __vsscanf(str, format, ap);
    va_end(ap);
    return (count);
}
