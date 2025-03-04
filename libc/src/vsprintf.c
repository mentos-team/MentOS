/// @file vsprintf.c
/// @brief Print formatting routines.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "ctype.h"
#include "fcvt.h"
#include "stdarg.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "sys/bitops.h"
#include "unistd.h"

/// Size of the buffer used to call cvt functions.
#define CVTBUFSIZE 500

#define FLAGS_ZEROPAD   (1U << 0U) ///< Fill zeros before the number.
#define FLAGS_LEFT      (1U << 1U) ///< Left align the value.
#define FLAGS_PLUS      (1U << 2U) ///< Print the plus sign.
#define FLAGS_SPACE     (1U << 3U) ///< If positive add a space instead of the plus sign.
#define FLAGS_HASH      (1U << 4U) ///< Preceed with 0x or 0X, %x or %X respectively.
#define FLAGS_UPPERCASE (1U << 5U) ///< Print uppercase.
#define FLAGS_SIGN      (1U << 6U) ///< Print the sign.

/// The list of digits.
static char *_digits = "0123456789abcdefghijklmnopqrstuvwxyz";

/// The list of uppercase digits.
static char *_upper_digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/// @brief Returns the integer value parsed from the beginning of the string
/// until a non-integer character is found.
/// @param s the string we need to analyze.
/// @return the integer value represented by the initial digits in the string.
/// @note This function assumes that `s` points to a valid string and will stop
/// parsing at the first non-integer character.
static inline int skip_atoi(const char **s)
{
    // Error check to ensure that the string pointer is valid.
    if (s == NULL || *s == NULL) {
        return -1;
    }

    int i = 0;

    // Iterate through the string as long as we encounter digits.
    while (isdigit(**s)) {
        // Convert the current digit character to its integer value.
        i = i * 10 + *((*s)++) - '0';
    }

    return i; // Return the parsed integer value.
}

/// @brief Transforms the number into a string.
/// @param str the output string.
/// @param end the end of the buffer to prevent overflow.
/// @param num the number to transform to string.
/// @param base the base to use for number transformation (e.g., 10 for decimal, 16 for hex).
/// @param size the minimum size of the output string (pads with '0' or spaces if necessary).
/// @param precision the precision for number conversion (affects floating point numbers and zero padding).
/// @param flags control flags (e.g., for padding, sign, and case sensitivity).
/// @return the resulting string after number transformation.
static char *number(char *str, char *end, long num, int base, int size, int32_t precision, unsigned flags)
{
    char tmp[66] = {0};     // Temporary buffer to hold the number in reverse order
    char *dig    = _digits; // Default digits for base conversion (lowercase)

    // Check for uppercase conversion flag.
    if (bitmask_check(flags, FLAGS_UPPERCASE)) {
        dig = _upper_digits; // Use uppercase digits if the flag is set
    }

    // Disable zero padding if left alignment is specified.
    if (bitmask_check(flags, FLAGS_LEFT)) {
        bitmask_clear_assign(flags, FLAGS_ZEROPAD);
    }

    // Error handling: base must be between 2 and 36.
    if (base < 2 || base > 36) {
        return 0; // Return NULL if the base is invalid.
    }

    // Set padding character based on the FLAGS_ZEROPAD flag (either '0' or ' ').
    const int paddingc = bitmask_check(flags, FLAGS_ZEROPAD) ? '0' : ' ';

    // Set the sign (for signed numbers).
    char sign = 0;
    if (bitmask_check(flags, FLAGS_SIGN)) {
        if (num < 0) {
            sign = '-';  // Negative sign for negative numbers
            num  = -num; // Make number positive
            size--;      // Reduce size for the sign
        } else if (bitmask_check(flags, FLAGS_PLUS)) {
            sign = '+'; // Positive sign for positive numbers
            size--;
        } else if (bitmask_check(flags, FLAGS_SPACE)) {
            sign = ' '; // Space for positive numbers
            size--;
        }
    }

    // Convert the number to unsigned for further processing.
    uint32_t uns_num = (uint32_t)num;

    // Handle the FLAGS_HASH for octal (base 8) and hexadecimal (base 16).
    if (bitmask_check(flags, FLAGS_HASH)) {
        if (base == 16) {
            size -= 2; // Hexadecimal prefix "0x" or "0X" uses 2 characters.
        } else if (base == 8) {
            size--; // Octal prefix "0" uses 1 character.
        }
    }

    // Convert the number to the target base.
    int32_t i = 0;
    if (uns_num == 0) {
        tmp[i++] = '0'; // Handle zero case explicitly.
    } else {
        while (uns_num != 0) {
            tmp[i++] = dig[((unsigned long)uns_num) % (unsigned)base]; // Convert to base
            uns_num  = ((unsigned long)uns_num) / (unsigned)base;      // Divide by base
        }
    }

    // Ensure precision is at least as large as the number's length.
    if (i > precision) {
        precision = i;
    }

    // Adjust the size based on the precision.
    size -= precision;

    // Write padding spaces if right-aligned and no zero padding.
    if (!bitmask_check(flags, FLAGS_ZEROPAD | FLAGS_LEFT)) {
        while (size-- > 0 && (end == NULL || str < end)) {
            *str++ = ' ';
        }
    }

    // Write the sign character if necessary.
    if (sign && (end == NULL || str < end)) {
        *str++ = sign;
    }

    // Write the prefix for octal and hexadecimal if the FLAGS_HASH is set.
    if (bitmask_check(flags, FLAGS_HASH)) {
        if (base == 8 && (end == NULL || str < end)) {
            *str++ = '0'; // Octal prefix.
        } else if (base == 16 && (str + 1) < end) {
            *str++ = '0';         // Hexadecimal prefix "0x".
            *str++ = _digits[33]; // 'x' or 'X' based on FLAGS_UPPERCASE.
        }
    }

    // Write zero-padding if necessary.
    if (!bitmask_check(flags, FLAGS_LEFT)) {
        while (size-- > 0 && (end == NULL || str < end)) {
            *str++ = paddingc; // Pad with '0' or ' '.
        }
    }

    // Write any additional zeros required by the precision.
    while (i < precision-- && (end == NULL || str < end)) {
        *str++ = '0';
    }

    // Write the number in reverse order (tmp array holds the reversed digits).
    while (i-- > 0 && (end == NULL || str < end)) {
        *str++ = tmp[i];
    }

    // If the number is left-aligned, pad remaining space with spaces.
    while (size-- > 0 && (end == NULL || str < end)) {
        *str++ = ' ';
    }

    return str; // Return the resulting string pointer.
}

/// @brief Converts a MAC address into a human-readable string format.
/// @param str The output string where the MAC address will be written.
/// @param end The end of the buffer to prevent overflow.
/// @param addr The 6-byte MAC address to be formatted.
/// @param size The minimum field width for the output (pads with spaces if necessary).
/// @param precision Unused in this function (for compatibility with similar functions).
/// @param flags Control flags that affect the format (e.g., uppercase and left alignment).
/// @return Pointer to the end of the output string.
static char *eaddr(char *str, char *end, unsigned char *addr, int size, int precision, unsigned flags)
{
    (void)precision; // Precision is unused in this function.

    char tmp[24];        // Temporary buffer to hold the formatted MAC address.
    char *dig = _digits; // Default digits for hex conversion (lowercase by default).
    int i;
    int len = 0;

    // Use uppercase hex digits if the FLAGS_UPPERCASE flag is set.
    if (bitmask_check(flags, FLAGS_UPPERCASE)) {
        dig = _upper_digits;
    }

    // Convert each byte of the MAC address to hex format.
    for (i = 0; i < 6; i++) {
        // Add colon separator between address bytes.
        if (i != 0) {
            if (len < sizeof(tmp)) {
                tmp[len++] = ':';
            }
        }

        if (len < sizeof(tmp)) {
            tmp[len++] = dig[addr[i] >> 4]; // Convert upper nibble to hex.
        }
        if (len < sizeof(tmp)) {
            tmp[len++] = dig[addr[i] & 0x0F]; // Convert lower nibble to hex.
        }
    }

    // Handle right alignment if the FLAGS_LEFT flag is not set.
    if (!bitmask_check(flags, FLAGS_LEFT)) {
        while (len < size-- && (end == NULL || str < end)) {
            *str++ = ' '; // Pad with spaces on the left if needed.
        }
    }

    // Copy the formatted MAC address from tmp buffer to the output string.
    for (i = 0; i < len && (end == NULL || str < end); ++i) {
        *str++ = tmp[i];
    }

    // Handle left padding if the FLAGS_LEFT flag is set.
    while (len < size-- && (end == NULL || str < end)) {
        *str++ = ' '; // Pad with spaces on the right if needed.
    }

    return str; // Return the pointer to the end of the output string.
}

/// @brief Converts an IPv4 address into a human-readable string format.
/// @param str The output string where the IPv4 address will be written.
/// @param end The end of the buffer to prevent overflow.
/// @param addr The 4-byte IPv4 address to be formatted.
/// @param size The minimum field width for the output (pads with spaces if necessary).
/// @param precision Unused in this function (for compatibility with similar functions).
/// @param flags Control flags that affect the format (e.g., left alignment).
/// @return Pointer to the end of the output string.
static char *iaddr(char *str, char *end, unsigned char *addr, int size, int precision, unsigned flags)
{
    (void)precision; // Precision is unused in this function.

    // Temporary buffer to hold the formatted IP address.
    char tmp[24];
    int i;
    int n;
    int len = 0;

    // Convert each byte of the IP address to decimal format.
    for (i = 0; i < 4; i++) {
        // Add a dot between each octet.
        if (i != 0 && len < sizeof(tmp)) {
            tmp[len++] = '.';
        }

        // Current octet of the IP address.
        n = addr[i];

        // Convert the current octet to decimal digits.
        if (n == 0) {
            // Handle the case where the octet is zero.
            if (len < sizeof(tmp)) {
                tmp[len++] = _digits[0];
            }
        } else {
            // If the number is greater than or equal to 100, we need to extract
            // hundreds, tens, and units.
            if (n >= 100 && len < sizeof(tmp)) {
                tmp[len++] = _digits[n / 100]; // Hundreds place.
                n          = n % 100;
            }
            if (n >= 10 && len < sizeof(tmp)) {
                tmp[len++] = _digits[n / 10]; // Tens place.
                n          = n % 10;
            }
            // Finally, add the unit digit.
            if (len < sizeof(tmp)) {
                tmp[len++] = _digits[n];
            }
        }
    }

    // Handle right alignment if the FLAGS_LEFT flag is not set.
    if (!bitmask_check(flags, FLAGS_LEFT)) {
        // Pad with spaces on the left if needed.
        while (len < size-- && (end == NULL || str < end)) {
            *str++ = ' ';
        }
    }

    // Copy the formatted IP address from tmp buffer to the output string.
    for (i = 0; i < len && (end == NULL || str < end); ++i) {
        *str++ = tmp[i];
    }

    // Handle left padding if the FLAGS_LEFT flag is set. Pad with spaces on the
    // right if needed.
    while (len < size-- && (end == NULL || str < end)) {
        *str++ = ' ';
    }

    return str; // Return the pointer to the end of the output string.
}

/// @brief Converts a floating-point number to a string with a specified format.
/// @param value The floating-point value to be converted.
/// @param buffer The output buffer to store the resulting string.
/// @param bufsize The size of the output buffer.
/// @param format The format specifier ('e', 'f', or 'g').
/// @param precision The number of digits to be displayed after the decimal
/// point.
static void cfltcvt(double value, char *buffer, size_t bufsize, char format, int precision)
{
    int decpt;
    int sign;
    int exp;
    int pos;
    char cvtbuf[CVTBUFSIZE]; // Temporary buffer to store the digits.
    char *digits = cvtbuf;   // Pointer to the digit buffer.
    int capexp   = 0;        // Flag to check for uppercase exponent.
    int magnitude;

    char *end = buffer + bufsize - 1; // Pointer to the end of the buffer.

    // Handle uppercase 'G' or 'E' format specifier.
    // Convert them to lowercase 'g' or 'e' for uniform processing.
    if (format == 'G' || format == 'E') {
        capexp = 1;       // Set capexp to handle uppercase exponent.
        format += 'a' - 'A'; // Convert to lowercase.
    }

    // Handle 'g' format: choose between 'e' or 'f' based on magnitude.
    if (format == 'g') {
        ecvtbuf(value, precision, &decpt, &sign, cvtbuf, CVTBUFSIZE);
        magnitude = decpt - 1; // Calculate magnitude of the number.

        // If magnitude is out of range for 'f', use scientific notation 'e'.
        if (magnitude < -4 || magnitude > precision - 1) {
            format = 'e';
            precision -= 1; // Adjust precision for 'e' format.
        } else {
            format = 'f';
            precision -= decpt; // Adjust precision for 'f' format.
        }
    }

    // Handle scientific notation ('e' format).
    if (format == 'e') {
        // Convert the number to scientific format.
        ecvtbuf(value, precision + 1, &decpt, &sign, cvtbuf, CVTBUFSIZE);

        // Add the sign to the output buffer if necessary.
        if (sign && buffer < end) {
            *buffer++ = '-';
        }

        // Add the first digit.
        if (buffer < end) {
            *buffer++ = *digits;
        }

        // Add the decimal point and remaining digits.
        if (precision > 0 && buffer < end) {
            *buffer++ = '.';
        }

        // Copy the remaining digits.
        for (int i = 1; i <= precision && buffer < end; i++) {
            *buffer++ = digits[i];
        }

        // Add the exponent character ('e' or 'E').
        if (buffer < end) {
            *buffer++ = capexp ? 'E' : 'e';
        }

        // Calculate the exponent.
        if (decpt == 0) {
            exp = (value == 0.0) ? 0 : -1;
        } else {
            exp = decpt - 1;
        }

        // Add the sign of the exponent.
        if (exp < 0 && buffer < end) {
            *buffer++ = '-';
            exp       = -exp;
        } else if (buffer < end) {
            *buffer++ = '+';
        }

        // Add the exponent digits (e.g., '01', '02').
        for (int i = 2; i >= 0 && buffer < end; i--) {
            buffer[i] = (char)((exp % 10) + '0');
            exp /= 10;
        }
        buffer += 3;
    }
    // Handle fixed-point notation ('f' format).
    else if (format == 'f') {
        // Convert the number to fixed-point format.
        fcvtbuf(value, precision, &decpt, &sign, cvtbuf, CVTBUFSIZE);

        // Add the sign to the output buffer if necessary.
        if (sign && buffer < end) {
            *buffer++ = '-';
        }

        // If digits exist, process them.
        if (*digits) {
            // Handle the case where the decimal point is before the first
            // digit.
            if (decpt <= 0) {
                if (buffer < end) {
                    *buffer++ = '0';
                }
                if (buffer < end) {
                    *buffer++ = '.';
                }

                // Add leading zeros before the first significant digit.
                for (pos = 0; pos < -decpt && buffer < end; pos++) {
                    *buffer++ = '0';
                }

                // Copy the digits.
                while (*digits && buffer < end) {
                    *buffer++ = *digits++;
                }
            }
            // Handle normal case where decimal point is after some digits.
            else {
                pos = 0;
                while (*digits && buffer < end) {
                    if (pos++ == decpt && buffer < end) {
                        *buffer++ = '.';
                    }
                    if (buffer < end) {
                        *buffer++ = *digits++;
                    }
                }
            }
        }
        // Handle case where the value is zero.
        else {
            if (buffer < end) {
                *buffer++ = '0';
            }
            if (precision > 0 && buffer < end) {
                *buffer++ = '.';
                for (pos = 0; pos < precision && buffer < end; pos++) {
                    *buffer++ = '0';
                }
            }
        }
    }

    if (buffer < end) {
        *buffer = '\0'; // Null-terminate the string.
    } else {
        *end = '\0'; // Ensure null-termination if buffer exceeded.
    }
}

/// @brief Ensures that a decimal point is present in the given number string.
/// @param buffer The string representation of a number.
/// @param bufsize The size of the output buffer.
static void forcdecpt(char *buffer, size_t bufsize)
{
    // Traverse the buffer to check if there is already a decimal point or an
    // exponent ('e' or 'E').
    char *end = buffer + bufsize - 1; // Pointer to the end of the buffer.
    while (*buffer && buffer < end) {
        if (*buffer == '.') {
            return; // Decimal point found, no need to modify.
        }
        if (*buffer == 'e' || *buffer == 'E') {
            break; // Reached exponent part of the number, stop checking.
        }
        buffer++; // Move to the next character.
    }

    // If an exponent ('e' or 'E') is found, shift the exponent part to make
    // space for the decimal point.
    if (*buffer && buffer < end) {
        // Get the length of the exponent part.
        long n = (long)strlen(buffer);

        // Check if there is enough space to shift and add the decimal point.
        if (buffer + n + 1 < end) {
            // Shift the buffer contents one position to the right to make space for
            // the decimal point.
            while (n >= 0) {
                buffer[n + 1] = buffer[n];
                n--;
            }

            // Insert the decimal point.
            *buffer = '.';
        }
    }
    // If no exponent is found, append the decimal point at the end of the string.
    else if (buffer < end) {
        *buffer++ = '.';  // Add decimal point at the current end.
        *buffer   = '\0'; // Null-terminate the string.
    }
}

/// @brief Removes trailing zeros after the decimal point in a number string.
/// @param buffer The string representation of a number.
/// @param bufsize The size of the output buffer.
static void cropzeros(char *buffer, size_t bufsize)
{
    char *stop;
    char *end = buffer + bufsize - 1; // Pointer to the end of the buffer.

    // Traverse until a decimal point is found or the end of the string is
    // reached.
    while (*buffer && *buffer != '.' && buffer < end) {
        buffer++;
    }

    // If there is a decimal point, find the position of the exponent or the end
    // of the number.
    if (*buffer++ && buffer < end) { // Move past the decimal point.
        // Continue until 'e', 'E', or end of string is found.
        while (*buffer && *buffer != 'e' && *buffer != 'E' && buffer < end) {
            buffer++;
        }

        // Store position of 'e', 'E', or end of the string, and backtrack one
        // step.
        stop = buffer--;

        // Backtrack over trailing zeros.
        while (*buffer == '0' && buffer > (buffer - bufsize)) {
            buffer--;
        }

        // If a decimal point is found with no significant digits after,
        // backtrack to remove it.
        if (*buffer == '.') {
            buffer--;
        }

        // Shift the string forward to overwrite any unnecessary characters
        // (trailing zeros or decimal point).
        while (buffer < end && (*++buffer = *stop++)) {
        }

        // Ensure null-termination if buffer exceeded.
        if (buffer >= end) {
            *end = '\0';
        }
    }
}

/// @brief Formats a floating-point number into a string with specified options.
///
/// @details This function converts a floating-point number into a string
/// representation based on the specified format, precision, and flags. It
/// handles alignment, padding, and sign appropriately.
///
/// @param str Pointer to the output string where the formatted number will be stored.
/// @param end Pointer to the end of the buffer to prevent overflow.
/// @param num The floating-point number to format.
/// @param size The total size of the output string, including padding.
/// @param precision The number of digits to display after the decimal point.
/// @param format The format specifier for the output ('f', 'g', 'e', etc.).
/// @param flags Control flags that modify the output format (e.g., left alignment, zero padding).
///
/// @return Pointer to the next position in the output string after the formatted number.
static char *flt(char *str, char *end, double num, int size, int precision, char format, unsigned flags)
{
    char workbuf[80];
    char c;
    char sign;
    int n;
    int i;

    /// If the `FLAGS_LEFT` is set, clear the `FLAGS_ZEROPAD` flag.
    /// Left alignment implies no zero padding.
    if (bitmask_check(flags, FLAGS_LEFT)) {
        bitmask_clear_assign(flags, FLAGS_ZEROPAD);
    }

    /// Determine the padding character (`c`) and the sign of the number.
    /// If `FLAGS_ZEROPAD` is set, the padding will be '0', otherwise it will be
    /// a space (' ').
    c    = bitmask_check(flags, FLAGS_ZEROPAD) ? '0' : ' ';
    sign = 0;

    /// Check the `FLAGS_SIGN` flag to determine if the sign should be added.
    if (bitmask_check(flags, FLAGS_SIGN)) {
        if (num < 0.0) {
            /// If the number is negative, set `sign` to '-' and make the number
            /// positive.
            sign = '-';
            num  = -num;
            size--; // Reduce size for the sign character.
        } else if (bitmask_check(flags, FLAGS_PLUS)) {
            /// If `FLAGS_PLUS` is set, prepend a '+' to positive numbers.
            sign = '+';
            size--;
        } else if (bitmask_check(flags, FLAGS_SPACE)) {
            /// If `FLAGS_SPACE` is set, prepend a space to positive numbers.
            sign = ' ';
            size--;
        }
    }

    /// Set the default precision if no precision is provided.
    if (precision < 0) {
        precision = 6; // Default precision is 6 decimal places.
    } else if (precision == 0 && format == 'g') {
        precision = 1; // For format 'g', default precision is 1.
    }

    /// Convert the floating-point number `num` into a string `workbuf` using the
    /// given format `format`.
    cfltcvt(num, workbuf, sizeof(workbuf), format, precision);

    /// If the `FLAGS_HASH` is set and precision is 0, force a decimal point in
    /// the output.
    if (bitmask_check(flags, FLAGS_HASH) && (precision == 0)) {
        forcdecpt(workbuf, sizeof(workbuf));
    }

    /// For format 'g', remove trailing zeros unless `FLAGS_HASH` is set.
    if ((format == 'g') && !bitmask_check(flags, FLAGS_HASH)) {
        cropzeros(workbuf, sizeof(workbuf));
    }

    /// Calculate the length of the resulting string `workbuf`.
    n = strnlen(workbuf, 80);

    /// Adjust size to account for the length of the output string.
    size -= n;

    /// Add padding spaces before the number if neither `FLAGS_ZEROPAD` nor `FLAGS_LEFT` are set.
    if (!bitmask_check(flags, FLAGS_ZEROPAD | FLAGS_LEFT)) {
        while (size-- > 0 && (end == NULL || str < end)) {
            *str++ = ' ';
        }
    }

    /// Add the sign character (if any) before the number.
    if (sign && (end == NULL || str < end)) {
        *str++ = sign;
    }

    /// Add padding characters (either '0' or spaces) before the number if `FLAGS_ZEROPAD` is set.
    if (!bitmask_check(flags, FLAGS_LEFT)) {
        while (size-- > 0 && (end == NULL || str < end)) {
            *str++ = c;
        }
    }

    /// Copy the formatted number string to the output `str`.
    for (i = 0; i < n && (end == NULL || str < end); i++) {
        *str++ = workbuf[i];
    }

    /// Add padding spaces after the number if `FLAGS_LEFT` is set (left-aligned output).
    while (size-- > 0 && (end == NULL || str < end)) {
        *str++ = ' ';
    }

    return str; /// Return the resulting string after formatting the number.
}

int vsprintf(char *str, const char *format, va_list args)
{
    int base;       // Base for number formatting.
    char *tmp;      // Pointer to current position in the output buffer.
    char *s;        // Pointer for string formatting.
    unsigned flags; // Flags for number formatting.
    char qualifier; // Character qualifier for integer fields ('h', 'l', or 'L').

    // Check for null input buffer or format string.
    if (str == NULL || format == NULL) {
        return -1; // Error: null pointer provided.
    }

    for (tmp = str; *format; format++) {
        if (*format != '%') {
            *tmp++ = *format; // Directly copy non-format characters.
            continue;
        }

        // Reset flags for each new format specifier.
        flags = 0;

    repeat:

        // Process format flags (skips first '%').
        format++;

        switch (*format) {
        case '-':
            bitmask_set_assign(flags, FLAGS_LEFT);
            goto repeat;
        case '+':
            bitmask_set_assign(flags, FLAGS_PLUS);
            goto repeat;
        case ' ':
            bitmask_set_assign(flags, FLAGS_SPACE);
            goto repeat;
        case '#':
            bitmask_set_assign(flags, FLAGS_HASH);
            goto repeat;
        case '0':
            bitmask_set_assign(flags, FLAGS_ZEROPAD);
            goto repeat;
        }

        // Get the width of the output field.
        int32_t field_width = -1;

        if (isdigit(*format)) {
            field_width = skip_atoi(&format);
        } else if (*format == '*') {
            format++;
            field_width = va_arg(args, int32_t);
            if (field_width < 0) {
                field_width = -field_width;
                bitmask_set_assign(flags, FLAGS_LEFT);
            }
        }

        // Get the precision, thus the minimum number of digits for integers;
        // max number of chars for from string.
        int32_t precision = -1;
        if (*format == '.') {
            ++format;
            if (isdigit(*format)) {
                precision = skip_atoi(&format);
            } else if (*format == '*') {
                ++format;
                precision = va_arg(args, int);
            }
            if (precision < 0) {
                precision = 0;
            }
        }

        // Get the conversion qualifier.
        qualifier = -1;
        if (*format == 'h' || *format == 'l' || *format == 'L') {
            qualifier = *format;
            format++;
        }

        // Default base for integer conversion.
        base = 10;

        switch (*format) {
        case 'c':
            // Handle left padding.
            if (!bitmask_check(flags, FLAGS_LEFT)) {
                while (--field_width > 0) {
                    *tmp++ = ' ';
                }
            }
            // Add the character.
            *tmp++ = va_arg(args, char);
            // Handle right padding.
            while (--field_width > 0) {
                *tmp++ = ' ';
            }
            continue;

        case 's':
            // Handle string formatting.
            s = va_arg(args, char *);
            if (!s) {
                s = "<NULL>";
            }
            // Get the length of the string.
            int32_t len = (int32_t)strnlen(s, (uint32_t)precision);
            // Handle left padding.
            if (!bitmask_check(flags, FLAGS_LEFT)) {
                while (len < field_width--) {
                    *tmp++ = ' ';
                }
            }
            // Add the string.
            for (int32_t it = 0; it < len; ++it) {
                *tmp++ = *s++;
            }
            // Handle right padding.
            while (len < field_width--) {
                *tmp++ = ' ';
            }
            continue;

        case 'p':

            // Handle pointer formatting.
            if (field_width == -1) {
                field_width = 2 * sizeof(void *);
                // Zero pad for pointers.
                bitmask_set_assign(flags, FLAGS_ZEROPAD);
            }
            tmp = number(tmp, NULL, (unsigned long)va_arg(args, void *), 16, field_width, precision, flags);
            continue;

        case 'n':
            // Handle writing the number of characters written.
            if (qualifier == 'l') {
                long *ip = va_arg(args, long *);
                *ip      = (tmp - str);
            } else {
                int *ip = va_arg(args, int *);
                *ip     = (tmp - str);
            }
            continue;

        case 'A':
            // Handle hexadecimal formatting with uppercase.
            bitmask_set_assign(flags, FLAGS_UPPERCASE);
            break;

        case 'a':
            // Handle address formatting (either Ethernet or IP).
            if (qualifier == 'l') {
                tmp = eaddr(tmp, NULL, va_arg(args, unsigned char *), field_width, precision, flags);
            } else {
                tmp = iaddr(tmp, NULL, va_arg(args, unsigned char *), field_width, precision, flags);
            }
            continue;

        case 'o':
            // Integer number formats.
            base = 8;
            break;

        case 'X':
            // Handle hexadecimal formatting with uppercase.
            bitmask_set_assign(flags, FLAGS_UPPERCASE);
            break;

        case 'x':
            // Handle hexadecimal formatting.
            base = 16;
            break;

        case 'd':
        case 'i':
            // Handle signed integer formatting.
            bitmask_set_assign(flags, FLAGS_SIGN);

        case 'u':
            // Handle unsigned integer formatting.
            break;

        case 'E':
        case 'G':
        case 'e':
        case 'f':
        case 'g':
            // Handle floating-point formatting.
            tmp = flt(tmp, NULL, va_arg(args, double), field_width, precision, *format, bitmask_set(flags, FLAGS_SIGN));
            continue;

        default:
            if (*format != '%') {
                *tmp++ = '%'; // Output '%' if not a format specifier.
            }
            if (*format) {
                *tmp++ = *format; // Output the current character.
            } else {
                --format; // Handle the case of trailing '%'.
            }
            continue;
        }

        // Process the integer value.
        if (bitmask_check(flags, FLAGS_SIGN)) {
            long num = (qualifier == 'l')   ? va_arg(args, long)
                       : (qualifier == 'h') ? va_arg(args, short)
                                            : va_arg(args, int);
            // Add the number.
            tmp      = number(tmp, NULL, num, base, field_width, precision, flags);
        } else {
            unsigned long num = (qualifier == 'l')   ? va_arg(args, unsigned long)
                                : (qualifier == 'h') ? va_arg(args, unsigned short)
                                                     : va_arg(args, unsigned int);
            // Add the number.
            tmp               = number(tmp, NULL, num, base, field_width, precision, flags);
        }
    }

    *tmp = '\0';             // Null-terminate the output string.
    return (int)(tmp - str); // Return the number of characters written.
}

int vsnprintf(char *str, size_t bufsize, const char *format, va_list args)
{
    int base;       // Base for number formatting.
    char *tmp;      // Pointer to current position in the output buffer.
    char *s;        // Pointer for string formatting.
    unsigned flags; // Flags for number formatting.
    char qualifier; // Character qualifier for integer fields ('h', 'l', or 'L').

    // Check for null input buffer or format string.
    if (str == NULL || format == NULL) {
        return -1; // Error: null pointer provided.
    }

    char *end = str + bufsize - 1; // Reserve space for null-terminator.

    for (tmp = str; *format && tmp < end; format++) {
        if (*format != '%') {
            if (tmp < end) {
                *tmp++ = *format; // Directly copy non-format characters.
            }
            continue;
        }

        // Reset flags for each new format specifier.
        flags = 0;

    repeat:

        // Process format flags (skips first '%').
        format++;

        switch (*format) {
        case '-':
            bitmask_set_assign(flags, FLAGS_LEFT);
            goto repeat;
        case '+':
            bitmask_set_assign(flags, FLAGS_PLUS);
            goto repeat;
        case ' ':
            bitmask_set_assign(flags, FLAGS_SPACE);
            goto repeat;
        case '#':
            bitmask_set_assign(flags, FLAGS_HASH);
            goto repeat;
        case '0':
            bitmask_set_assign(flags, FLAGS_ZEROPAD);
            goto repeat;
        }

        // Get the width of the output field.
        int32_t field_width = -1;

        if (isdigit(*format)) {
            field_width = skip_atoi(&format);
        } else if (*format == '*') {
            format++;
            field_width = va_arg(args, int32_t);
            if (field_width < 0) {
                field_width = -field_width;
                bitmask_set_assign(flags, FLAGS_LEFT);
            }
        }

        // Get the precision, thus the minimum number of digits for integers;
        // max number of chars for from string.
        int32_t precision = -1;
        if (*format == '.') {
            ++format;
            if (isdigit(*format)) {
                precision = skip_atoi(&format);
            } else if (*format == '*') {
                ++format;
                precision = va_arg(args, int);
            }
            if (precision < 0) {
                precision = 0;
            }
        }

        // Get the conversion qualifier.
        qualifier = -1;
        if (*format == 'h' || *format == 'l' || *format == 'L') {
            qualifier = *format;
            format++;
        }

        // Default base for integer conversion.
        base = 10;

        switch (*format) {
        case 'c':
            // Handle left padding.
            if (!bitmask_check(flags, FLAGS_LEFT)) {
                while (--field_width > 0 && tmp < end) {
                    *tmp++ = ' ';
                }
            }
            // Add the character.
            if (tmp < end) {
                *tmp++ = va_arg(args, int);
            }
            // Handle right padding.
            while (--field_width > 0 && tmp < end) {
                *tmp++ = ' ';
            }
            continue;

        case 's':
            // Handle string formatting.
            s = va_arg(args, char *);
            if (!s) {
                s = "<NULL>";
            }
            // Get the length of the string.
            int32_t len = (int32_t)strnlen(s, (uint32_t)precision);
            // Handle left padding.
            if (!bitmask_check(flags, FLAGS_LEFT)) {
                while (len < field_width-- && tmp < end) {
                    *tmp++ = ' ';
                }
            }
            // Add the string.
            for (int32_t it = 0; it < len && tmp < end; ++it) {
                *tmp++ = *s++;
            }
            // Handle right padding.
            while (len < field_width-- && tmp < end) {
                *tmp++ = ' ';
            }
            continue;

        case 'p':
            // Handle pointer formatting.
            if (field_width == -1) {
                field_width = 2 * sizeof(void *);
                // Zero pad for pointers.
                bitmask_set_assign(flags, FLAGS_ZEROPAD);
            }
            tmp = number(tmp, end, (unsigned long)va_arg(args, void *), 16, field_width, precision, flags);
            continue;

        case 'n':
            // Handle writing the number of characters written.
            if (qualifier == 'l') {
                long *ip = va_arg(args, long *);
                *ip      = (tmp - str);
            } else {
                int *ip = va_arg(args, int *);
                *ip     = (tmp - str);
            }
            continue;

        case 'A':
            // Handle hexadecimal formatting with uppercase.
            bitmask_set_assign(flags, FLAGS_UPPERCASE);
            break;

        case 'a':
            // Handle address formatting (either Ethernet or IP).
            if (qualifier == 'l') {
                tmp = eaddr(tmp, end, va_arg(args, unsigned char *), field_width, precision, flags);
            } else {
                tmp = iaddr(tmp, end, va_arg(args, unsigned char *), field_width, precision, flags);
            }
            continue;

        case 'o':
            // Integer number formats.
            base = 8;
            break;

        case 'X':
            // Handle hexadecimal formatting with uppercase.
            bitmask_set_assign(flags, FLAGS_UPPERCASE);
            // Fall through.
        case 'x':
            // Handle hexadecimal formatting.
            base = 16;
            break;

        case 'd':
        case 'i':
            // Handle signed integer formatting.
            bitmask_set_assign(flags, FLAGS_SIGN);
            // Fall through.
        case 'u':
            // Handle unsigned integer formatting.
            break;

        case 'E':
        case 'G':
        case 'e':
        case 'f':
        case 'g':
            // Handle floating-point formatting.
            tmp = flt(tmp, end, va_arg(args, double), field_width, precision, *format, bitmask_set(flags, FLAGS_SIGN));
            continue;

        default:
            if (*format != '%') {
                if (tmp < end) {
                    *tmp++ = '%'; // Output '%' if not a format specifier.
                }
            }
            if (*format && tmp < end) {
                *tmp++ = *format; // Output the current character.
            } else {
                --format; // Handle the case of trailing '%'.
            }
            continue;
        }

        // Process the integer value.
        if (bitmask_check(flags, FLAGS_SIGN)) {
            long num = (qualifier == 'l')   ? va_arg(args, long)
                       : (qualifier == 'h') ? va_arg(args, short)
                                            : va_arg(args, int);
            // Add the number.
            tmp      = number(tmp, end, num, base, field_width, precision, flags);
        } else {
            unsigned long num = (qualifier == 'l')   ? va_arg(args, unsigned long)
                                : (qualifier == 'h') ? va_arg(args, unsigned short)
                                                     : va_arg(args, unsigned int);
            // Add the number.
            tmp               = number(tmp, end, num, base, field_width, precision, flags);
        }
    }

    if (tmp < end) {
        *tmp = '\0'; // Null-terminate the output string.
    } else {
        *end = '\0'; // Ensure null-termination if buffer exceeded.
    }
    return (int)(tmp - str); // Return the number of characters written.
}

int printf(const char *format, ...)
{
    char buffer[4096] = {0};
    va_list ap;
    int len;
    // Start variabile argument's list.
    va_start(ap, format);
    len = vsnprintf(buffer, 4096, format, ap);
    va_end(ap);
    puts(buffer);
    return len;
}

int sprintf(char *str, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    int len = vsprintf(str, format, ap);
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
    int len = vsprintf(buffer, format, args);

    if (len > 0) {
        if (write(fd, buffer, len) <= 0) {
            return EOF;
        }
        return len;
    }
    return -1;
}

int fprintf(int fd, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(fd, format, ap);
    va_end(ap);

    return ret;
}
