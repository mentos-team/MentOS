/// @file vsprintf.c
/// @brief Print formatting routines.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "math.h"
#include "stdarg.h"
#include "stddef.h"
#include "stdio.h"
#include "unistd.h"

#define FLAGS_ZEROPAD   (1U << 0U) ///< Fill zeros before the number.
#define FLAGS_LEFT      (1U << 1U) ///< Left align the value.
#define FLAGS_PLUS      (1U << 2U) ///< Print the plus sign.
#define FLAGS_SPACE     (1U << 3U) ///< If positive add a space instead of the plus sign.
#define FLAGS_UPPERCASE (1U << 4U) ///< Print uppercase.
#define FLAGS_SIGN      (1U << 5U) ///< Print the sign.
#define FLAGS_NEGATIVE  (1U << 6U) ///< Negative number flag.

/// @brief Internal function to emit a character.
/// @param buf Current pointer in the buffer.
/// @param end Pointer to the end of the buffer.
/// @param c The character to emit.
static inline void __emit_char(char **buf, char *end, char c)
{
    if (*buf && *buf < end) {
        *(*buf)++ = c;
    } else if (*buf) {
        (*buf)++;
    }
}

/// @brief Internal function to emit padding characters.
/// @param buf Current pointer in the buffer.
/// @param end Pointer to the end of the buffer.
/// @param padding The number of padding characters to emit.
/// @param padchar The character to use for padding.
static void __emit_padding(char **buf, char *end, int padding, char padchar)
{
    while (padding-- > 0) {
        __emit_char(buf, end, padchar);
    }
}

/// @brief Internal function to emit a number in a specific base.
/// @param buffer Buffer to store the number.
/// @param buflen Length of the buffer.
/// @param num The number to format.
/// @param base The base to use for formatting (e.g., 10 for decimal, 16 for hexadecimal).
/// @param precision The minimum number of digits to print.
/// @param flags Formatting flags.
/// @return The length of the formatted number.
static int __emit_number(char *buffer, size_t buflen, unsigned long num, int base, int precision, int flags)
{
    size_t len = 0;
    // Reserve space for prefix.
    buflen -= (flags & (FLAGS_NEGATIVE | FLAGS_PLUS | FLAGS_SPACE));
    // Convert number to string (in reverse).
    do {
        buffer[len++] = ((flags & FLAGS_UPPERCASE) ? "0123456789ABCDEF" : "0123456789abcdef")[num % base];
        num /= base;
    } while ((num > 0) && (len < buflen));
    // Apply precision (zero padding).
    if (precision > 0) {
        while ((len < precision) && (len < buflen)) {
            buffer[len++] = '0';
        }
    }
    // Handle sign/prefix.
    if (flags & FLAGS_NEGATIVE) {
        buffer[len++] = '-';
    } else if (flags & FLAGS_PLUS) {
        buffer[len++] = '+';
    } else if (flags & FLAGS_SPACE) {
        buffer[len++] = ' ';
    }
    return len;
}

/// @brief Internal function to handle string formatting.
/// @param buf Current pointer in the buffer.
/// @param end Pointer to the end of the buffer.
/// @param str The string to format.
/// @param width The minimum width of the output.
/// @param precision The maximum number of characters to print.
/// @param flags Formatting flags.
static void __format_string(char **buf, char *end, const char *str, int width, int precision, int flags)
{
    int len       = 0;
    const char *s = str;
    // If precision is set, limit the length to precision.
    while (*s && (precision < 0 || len < precision)) {
        len++;
        s++;
    }
    // Calculate remaining width.
    int padding = width - len;
    // Apply **right padding** (default behavior, spaces before content)
    if (!(flags & FLAGS_LEFT)) {
        __emit_padding(buf, end, padding, ' ');
    }
    // Copy the string into the buffer.
    while (*str && (len-- > 0)) {
        __emit_char(buf, end, *str++);
    }
    // Apply **left padding** only if FLAGS_LEFT is set.
    if (flags & FLAGS_LEFT) {
        __emit_padding(buf, end, padding, ' ');
    }
}

/// @brief Internal function to handle character formatting.
/// @param buf Current pointer in the buffer.
/// @param end Pointer to the end of the buffer.
/// @param c The character to format.
/// @param width The minimum width of the output.
/// @param flags Formatting flags.
static void __format_char(char **buf, char *end, char c, int width, int flags)
{
    // Calculate remaining width.
    int padding = width - 1;
    // Apply right padding before the character if right-aligned.
    if (!(flags & FLAGS_LEFT)) {
        __emit_padding(buf, end, padding, ' ');
    }
    // Insert character.
    __emit_char(buf, end, c);
    // Apply left padding only if FLAGS_LEFT is set.
    if (flags & FLAGS_LEFT) {
        __emit_padding(buf, end, padding, ' ');
    }
}

/// @brief Internal function to handle integer formatting.
/// @param buf Current pointer in the buffer.
/// @param end Pointer to the end of the buffer.
/// @param num The number to format.
/// @param base The base to use for formatting (e.g., 10 for decimal, 16 for hexadecimal).
/// @param width The minimum width of the output.
/// @param precision The minimum number of digits to print.
/// @param flags Formatting flags.
static void __format_integer(char **buf, char *end, long num, int base, int width, int precision, int flags)
{
    char tmp[32] = {0};
    unsigned long unum;
    if (num < 0) {
        unum = -num;
        flags |= FLAGS_NEGATIVE;
    } else {
        unum = num;
    }
    // Convert number to string in reverse.
    int len     = __emit_number(tmp, sizeof(tmp) - 1, unum, 10, precision, flags);
    // Calculate remaining width.
    int padding = width - len;
    // Apply right padding before the character if right-aligned.
    if (!(flags & FLAGS_LEFT)) {
        __emit_padding(buf, end, padding, (flags & FLAGS_ZEROPAD) ? '0' : ' ');
    }
    // Copy the converted number in correct order.
    while (len-- > 0) {
        __emit_char(buf, end, tmp[len]);
    }
    // Apply left padding only if FLAGS_LEFT is set.
    if (flags & FLAGS_LEFT) {
        __emit_padding(buf, end, padding, ' ');
    }
}

/// @brief Internal function to handle unsigned integer formatting.
/// @param buf Current pointer in the buffer.
/// @param end Pointer to the end of the buffer.
/// @param num The number to format.
/// @param base The base to use for formatting (e.g., 10 for decimal, 16 for hexadecimal).
/// @param width The minimum width of the output.
/// @param precision The minimum number of digits to print.
/// @param flags Formatting flags.
static void __format_unsigned(char **buf, char *end, unsigned long num, int base, int width, int precision, int flags)
{
    char tmp[32] = {0};
    // Convert number to string (reverse order).
    int len      = __emit_number(tmp, sizeof(tmp) - 1, num, base, precision, flags);
    // Calculate remaining width.
    int padding  = width - len;
    // Apply right padding before the character if right-aligned.
    if (!(flags & FLAGS_LEFT)) {
        __emit_padding(buf, end, padding, (flags & FLAGS_ZEROPAD) ? '0' : ' ');
    }
    // Copy number to buffer in correct order.
    while (len-- > 0) {
        __emit_char(buf, end, tmp[len]);
    }
    // Apply left padding only if FLAGS_LEFT is set.
    if (flags & FLAGS_LEFT) {
        __emit_padding(buf, end, padding, ' ');
    }
}

/// @brief Internal function to handle floating-point formatting.
/// @param buf Current pointer in the buffer.
/// @param end Pointer to the end of the buffer.
/// @param num The floating-point number to format.
/// @param width The minimum width of the output.
/// @param precision The number of digits after the decimal point.
/// @param flags Formatting flags.
static void __format_float(char **buf, char *end, double num, int width, int precision, int flags)
{
    // Default precision for %f.
    if (precision < 0) {
        precision = 6;
    }
    // Handle sign.
    if (num < 0) {
        __emit_char(buf, end, '-');
        num = -num;
    }
    // Extract integer and decimal parts.
    long whole      = (long)num;
    double fraction = num - whole;
    fraction        = round(fraction * pow(10, precision));
    // Print whole part.
    __format_integer(buf, end, whole, 10, 0, 0, flags);
    // Print decimal point and fraction part.
    if (precision > 0) {
        __emit_char(buf, end, '.');
        __format_integer(buf, end, (long)fraction, 10, precision, precision, 0);
    }
}

/// @brief Internal function to handle pointer formatting.
/// @param buf Current pointer in the buffer.
/// @param end Pointer to the end of the buffer.
/// @param ptr The pointer to format.
/// @param width The minimum width of the output.
/// @param flags Formatting flags.
static void __format_pointer(char **buf, char *end, void *ptr, int width, int flags)
{
    unsigned long addr = (unsigned long)ptr;
    // Prefix `0x` for pointer formatting.
    __emit_char(buf, end, '0');
    __emit_char(buf, end, 'x');
    __format_unsigned(buf, end, addr, 16, width - 2, 0, 0);
}

/// @brief Handles `%n`, storing the number of characters printed so far.
/// @param count_var Pointer to the integer where the character count is stored.
/// @param count The number of characters written so far.
static void __format_count(int *count_var, int count)
{
    if (count_var) {
        *count_var = count;
    }
}

int vsnprintf(char *buffer, size_t size, const char *format, va_list args)
{
    char dummy;
    char *buf = buffer;
    char *end = buffer + size - 1;

    // Handle NULL buffer case.
    if (!buffer && size == 0) {
        buffer = &dummy;
        buf    = &dummy;
        end    = &dummy;
    }

    // Tracks number of characters written.
    int count = 0;

    while (*format) {
        if (*format == '%') {
            format++; // Skip '%'

            int flags     = 0;
            int width     = 0;
            int precision = -1; // Default: no precision specified
            int length    = 0;  // Length modifier (h, l, ll, etc.)

            // Step 1: Parse Flags.
            while (*format == '-' || *format == '+' || *format == ' ' || *format == '#' || *format == '0') {
                switch (*format) {
                case '-':
                    flags |= FLAGS_LEFT;
                    break;
                case '+':
                    flags |= FLAGS_PLUS;
                    break;
                case ' ':
                    flags |= FLAGS_SPACE;
                    break;
                case '0':
                    flags |= FLAGS_ZEROPAD;
                    break;
                }
                format++;
            }

            // Step 2: Parse Width.
            if (*format == '*') {
                width = va_arg(args, int);
                format++;
            } else {
                while (*format >= '0' && *format <= '9') {
                    width = width * 10 + (*format - '0');
                    format++;
                }
            }

            // Step 3: Parse Precision.
            if (*format == '.') {
                format++;
                if (*format == '*') {
                    precision = va_arg(args, int);
                    format++;
                } else {
                    precision = 0;
                    while (*format >= '0' && *format <= '9') {
                        precision = precision * 10 + (*format - '0');
                        format++;
                    }
                }
            }

            // Step 4: Parse Length Modifier.
            if (*format == 'h') {
                format++;
                // "hh" (char)
                if (*format == 'h') {
                    length = 2;
                    format++;
                }
                // "h" (short)
                else {
                    length = 1;
                }
            } else if (*format == 'l') {
                format++;
                // "ll" (long)
                if (*format == 'l') {
                    length = 4;
                    format++;
                }
                // "l" (long)
                else {
                    length = 3;
                }
            }

            // Enable uppercase flag if necessary.
            flags |= ((*format == 'X') ? FLAGS_UPPERCASE : 0);

            // Step 5: Parse Specifier and Call Handler.
            switch (*format) {
            case 's': {
                __format_string(&buf, end, va_arg(args, const char *), width, precision, flags);
                break;
            }
            case 'c': {
                __format_char(&buf, end, (char)va_arg(args, int), width, flags);
                break;
            }
            case 'd':
            case 'i': {
                long num;
                if (length == 0) {
                    num = va_arg(args, int);
                } else if (length == 1) {
                    num = (short)va_arg(args, int);
                } else if (length == 2) {
                    num = (char)va_arg(args, int);
                } else {
                    num = va_arg(args, long);
                }
                __format_integer(&buf, end, num, 10, width, precision, flags);
                break;
            }
            case 'u':
            case 'o':
            case 'x':
            case 'X': {
                unsigned long num;
                if (length == 0) {
                    num = va_arg(args, unsigned int);
                } else if (length == 1) {
                    num = (unsigned short)va_arg(args, unsigned int);
                } else if (length == 2) {
                    num = (unsigned char)va_arg(args, unsigned int);
                } else {
                    num = va_arg(args, unsigned long);
                }
                int base;
                if (*format == 'o') {
                    base = 8;
                } else if (*format == 'x' || *format == 'X') {
                    base = 16;
                } else {
                    base = 10;
                }
                __format_unsigned(&buf, end, num, base, width, precision, flags);
                break;
            }
            case 'p': {
                __format_pointer(&buf, end, va_arg(args, void *), width, flags);
                break;
            }
            case 'f':
            case 'F': {
                __format_float(&buf, end, va_arg(args, double), width, precision, flags);
                break;
            }
            case 'n': {
                __format_count(va_arg(args, int *), count);
                break;
            }
            case '%': {
                __emit_char(&buf, end, '%');
                break;
            }
            default: {
                __emit_char(&buf, end, '%');
                __emit_char(&buf, end, *format);
                break;
            }
            }
        } else {
            __emit_char(&buf, end, *format);
        }

        format++;
        count++;
    }
    if (buffer)
        *buf = '\0';
    return buf - buffer;
}

int vsprintf(char *str, const char *format, va_list args) { return vsnprintf(str, 4096, format, args); }

int printf(const char *format, ...)
{
    char buffer[4096];
    va_list ap;
    int len;

    va_start(ap, format);
    len = vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    if (len > 0) {
        write(STDOUT_FILENO, buffer, len);
    }

    return len;
}

int sprintf(char *str, const char *format, ...)
{
    va_list ap;
    int len;
    va_start(ap, format);
    len = vsnprintf(str, 4096, format, ap);
    va_end(ap);
    return len;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;
    int len;
    va_start(args, format);
    len = vsnprintf(str, size, format, args);
    va_end(args);
    return len;
}

int vfprintf(int fd, const char *format, va_list args)
{
    char buffer[4096];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    if (len > 0) {
        if (write(fd, buffer, len) <= 0) {
            return EOF;
        }
    }
    return len;
}

int fprintf(int fd, const char *format, ...)
{
    va_list ap;
    int len;
    va_start(ap, format);
    len = vfprintf(fd, format, ap);
    va_end(ap);
    return len;
}
