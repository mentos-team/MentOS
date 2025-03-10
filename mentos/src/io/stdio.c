/// @file stdio.c
/// @brief Standard IO functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stdio.h"
#include "ctype.h"
#include "drivers/keyboard/keyboard.h"
#include "errno.h"
#include "io/video.h"
#include "string.h"
#include "system/syscall.h"

int atoi(const char *str)
{
    // Check the input string.
    if (str == NULL) {
        return 0;
    }
    // Initialize sign, the result variable, and two indices.
    int sign   = (str[0] == '-') ? -1 : +1;
    int result = 0;
    int i;
    // Find where the number ends.
    for (i = (sign == -1) ? 1 : 0; (str[i] != '\0') && isdigit(str[i]); ++i) {
        result = (result * 10) + str[i] - '0';
    }
    return sign * result;
}

long strtol(const char *str, char **endptr, int base)
{
    const char *s;
    long acc;
    long cutoff;
    int c;
    int neg;
    int any;
    int cutlim;
    /*
     * Skip white space and pick up leading +/- sign if any.
     * If base is 0, allow 0x for hex and 0 for octal, else
     * assume decimal; if base is already 16, allow 0x.
     */
    s = str;
    do {
        c = (unsigned char)*s++;
    } while (isspace(c));
    if (c == '-') {
        neg = 1;
        c   = *s++;
    } else {
        neg = 0;
        if (c == '+') {
            c = *s++;
        }
    }
    if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0) {
        base = c == '0' ? 8 : 10;
    }
    /*
     * Compute the cutoff value between legal numbers and illegal
     * numbers.  That is the largest legal value, divided by the
     * base.  An input number that is greater than this value, if
     * followed by a legal input character, is too big.  One that
     * is equal to this value may be valid or not; the limit
     * between valid and invalid numbers is then based on the last
     * digit.  For instance, if the range for longs is
     * [-2147483648..2147483647] and the input base is 10,
     * cutoff will be set to 214748364 and cutlim to either
     * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
     * a value > 214748364, or equal but the next digit is > 7 (or 8),
     * the number is too big, and we will return a range error.
     *
     * Set any if any `digits' consumed; make it negative to indicate
     * overflow.
     */
    cutoff = neg ? LONG_MIN : LONG_MAX;
    cutlim = cutoff % base;
    cutoff /= base;
    if (neg) {
        if (cutlim > 0) {
            cutlim -= base;
            cutoff += 1;
        }
        cutlim = -cutlim;
    }
    for (acc = 0, any = 0;; c = (unsigned char)*s++) {
        if (isdigit(c)) {
            c -= '0';
        } else if (isalpha(c)) {
            c -= isupper(c) ? 'A' - 10 : 'a' - 10;
        } else {
            break;
        }
        if (c >= base) {
            break;
        }
        if (any < 0) {
            continue;
        }
        if (neg) {
            if (acc < cutoff || (acc == cutoff && c > cutlim)) {
                any   = -1;
                acc   = LONG_MIN;
                errno = ERANGE;
            } else {
                any = 1;
                acc *= base;
                acc -= c;
            }
        } else {
            if (acc > cutoff || (acc == cutoff && c > cutlim)) {
                any   = -1;
                acc   = LONG_MAX;
                errno = ERANGE;
            } else {
                any = 1;
                acc *= base;
                acc += c;
            }
        }
    }
    if (endptr != 0) {
        *endptr = (char *)(any ? s - 1 : str);
    }
    return (acc);
}
