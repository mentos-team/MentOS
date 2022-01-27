/// @file fcvt.c
/// @brief Define the functions required to turn double values into a string.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fcvt.h"
#include "math.h"

static void cvt(double arg, int ndigits, int *decpt, int *sign, char *buf, unsigned buf_size, int eflag)
{
    int r2;
    double fi, fj;
    char *p, *p1;

    char *buf_end = (buf + buf_size);

    if (ndigits < 0) {
        ndigits = 0;
    }

    if (ndigits >= buf_size - 1) {
        ndigits = buf_size - 2;
    }

    r2    = 0;
    *sign = 0;
    p     = &buf[0];

    if (arg < 0) {
        *sign = 1;
        arg   = -arg;
    }

    arg = modf(arg, &fi);
    p1  = buf_end;

    if (fi != 0) {
        p1 = buf_end;
        while (fi != 0) {
            fj    = modf(fi / 10, &fi);
            *--p1 = (int)((fj + .03) * 10) + '0';
            r2++;
        }
        while (p1 < buf_end) {
            *p++ = *p1++;
        }
    } else if (arg > 0) {
        while ((fj = arg * 10) < 1) {
            arg = fj;
            r2--;
        }
    }

    p1 = &buf[ndigits];
    if (eflag == 0) {
        p1 += r2;
    }

    *decpt = r2;
    if (p1 < &buf[0]) {
        buf[0] = '\0';
        return;
    }

    while (p <= p1 && p < buf_end) {
        arg *= 10;
        arg  = modf(arg, &fj);
        *p++ = (int)fj + '0';
    }

    if (p1 >= buf_end) {
        buf[buf_size - 1] = '\0';
        return;
    }

    p = p1;
    *p1 += 5;
    while (*p1 > '9') {
        *p1 = '0';
        if (p1 > buf) {
            ++*--p1;
        } else {
            *p1 = '1';
            (*decpt)++;
            if (eflag == 0) {
                if (p > buf)
                    *p = '0';
                p++;
            }
        }
    }
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
