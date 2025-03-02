/// @file fcvt.c
/// @brief Define the functions required to turn double values into a string.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[FCVT]  "      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "fcvt.h"
#include "math.h"

/// @brief Converts a floating-point number into a string representation.
/// @param arg The floating-point number to convert.
/// @param ndigits The number of digits to generate after the decimal point.
/// @param decpt A pointer to an integer that will store the position of the decimal point.
/// @param sign A pointer to an integer that will store the sign of the number (1 if negative, 0 otherwise).
/// @param buf A character buffer where the resulting string will be stored.
/// @param buf_size The size of the buffer.
/// @param eflag A flag indicating whether to use exponential notation (1 for exponential, 0 for standard).
static void cvt(double arg, int ndigits, int *decpt, int *sign, char *buf, unsigned buf_size, int eflag)
{
    // Error check: Ensure the buffer is valid and has enough space
    if (!buf || buf_size == 0) {
        pr_err("Invalid buffer or buffer size.\n");
        return;
    }

    int r2;
    double fi, fj;
    char *p, *p1;

    char *buf_end = (buf + buf_size);

    // Adjust the number of digits if it's negative or larger than buffer size
    if (ndigits < 0) {
        ndigits = 0;
    }

    // Ensure there's space for the null terminator.
    if (ndigits >= buf_size - 1) {
        ndigits = buf_size - 2;
    }

    r2    = 0;
    *sign = 0;
    p     = &buf[0];

    // Handle negative numbers by adjusting the sign and converting to positive
    if (arg < 0) {
        *sign = 1;
        arg   = -arg;
    }

    // Split the number into its integer and fractional parts.
    arg = modf(arg, &fi);

    // Start filling from the end of the buffer
    p1 = buf_end;

    // Process the integer part (if non-zero).
    if (fi != 0) {
        p1 = buf_end;

        // Prevent buffer overflow.
        while ((fi != 0) && (p1 > buf)) {
            fj = modf(fi / 10, &fi);

            // Convert each digit.
            *--p1 = (int)((fj + .03) * 10) + '0';

            // Increment the position of the decimal point.
            r2++;
        }
        // Copy the integer part to the buffer.
        while (p1 < buf_end) {
            *p++ = *p1++;
        }
    } else if (arg > 0) {
        // Handle fractional parts when there's no integer part.
        while ((fj = arg * 10) < 1) {
            arg = fj;

            // Shift the decimal point for fractional values.
            r2--;
        }
    }

    // Prepare to process the fractional part
    p1 = &buf[ndigits];

    // Adjust for standard notation.
    if (eflag == 0) {
        p1 += r2;
    }

    // Store the decimal point position.
    *decpt = r2;

    // If the buffer is too small, terminate early.
    if (p1 < &buf[0]) {
        buf[0] = '\0';
        return;
    }

    // Process the fractional part and fill the buffer.
    while (p <= p1 && p < buf_end) {
        arg *= 10;
        arg  = modf(arg, &fj);
        *p++ = (int)fj + '0';
    }

    // Ensure we terminate the string if it exceeds the buffer.
    if (p1 >= buf_end) {
        buf[buf_size - 1] = '\0';
        return;
    }

    // Round the last digit and handle carry propagation.
    p = p1;
    *p1 += 5;
    while (*p1 > '9') {
        // Carry propagation if rounding overflows.
        *p1 = '0';

        // Propagate carry to the previous digit.
        if (p1 > buf) {
            ++*--p1;
        } else {
            // Handle overflow at the beginning.
            *p1 = '1';

            // Increment the decimal point.
            (*decpt)++;

            // Pad with zeros if needed.
            if (eflag == 0) {
                if (p > buf) {
                    *p = '0';
                }
                p++;
            }
        }
    }

    // Terminate the string.
    *p = '\0';
}

void ecvtbuf(double arg, int chars, int *decpt, int *sign, char *buf, unsigned buf_size)
{
    cvt(arg, chars, decpt, sign, buf, buf_size, 1);
}

void fcvtbuf(double arg, int decimals, int *decpt, int *sign, char *buf, unsigned buf_size)
{
    cvt(arg, decimals, decpt, sign, buf, buf_size, 0);
}
