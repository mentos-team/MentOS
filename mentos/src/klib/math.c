/// @file math.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "math.h"
#include "stdint.h"

double round(double x)
{
    double out;
    __asm__ __volatile__("fldln2; fldl %1; frndint"
                         : "=t"(out)
                         : "m"(x));
    return out;
}

double floor(double x)
{
    if (x > -1.0 && x < 1.0) {
        if (x >= 0)
            return 0.0;
        return -1.0;
    }
    int i = (int)x;
    if (x < 0)
        return (double)(i - 1);
    return (double)i;
}

double ceil(double x)
{
    if (x > -1.0 && x < 1.0) {
        if (x <= 0)
            return 0.0;
        return 1.0;
    }
    int i = (int)x;
    if (x > 0)
        return (double)(i + 1);
    return (double)i;
}

double pow(double base, double exponent)
{
    double out;
    __asm__ __volatile__("fyl2x;"
                         "fld %%st;"
                         "frndint;"
                         "fsub %%st,%%st(1);"
                         "fxch;"
                         "fchs;"
                         "f2xm1;"
                         "fld1;"
                         "faddp;"
                         "fxch;"
                         "fld1;"
                         "fscale;"
                         "fstp %%st(1);"
                         "fmulp;"
                         : "=t"(out)
                         : "0"(base), "u"(exponent)
                         : "st(1)");
    return out;
}

double exp(double x)
{
    return pow(M_E, x);
}

double fabs(double x)
{
    double out;
    __asm__ __volatile__("fldln2; fldl %1; fabs"
                         : "=t"(out)
                         : "m"(x));
    return out;
}

float fabsf(float x)
{
    float out;
    __asm__ __volatile__("fldln2; fldl %1; fabs"
                         : "=t"(out)
                         : "m"(x));
    return out;
}

double sqrt(double x)
{
    double out;
    __asm__ __volatile__("fldln2; fldl %1; fsqrt"
                         : "=t"(out)
                         : "m"(x));
    return out;
}

float sqrtf(float x)
{
    float out;
    __asm__ __volatile__("fldln2; fldl %1; fsqrt"
                         : "=t"(out)
                         : "m"(x));
    return out;
}

int isinf(double x)
{
    union {
        unsigned long long u;
        double f;
    } ieee754;
    ieee754.f = x;
    return ((unsigned)(ieee754.u >> 32U) & 0x7fffffffU) == 0x7ff00000U &&
           ((unsigned)ieee754.u == 0);
}

int isnan(double x)
{
    union {
        unsigned long long u;
        double f;
    } ieee754;
    ieee754.f = x;
    return ((unsigned)(ieee754.u >> 32) & 0x7fffffff) +
               ((unsigned)ieee754.u != 0) >
           0x7ff00000;
}

double log10(double x)
{
    return ln(x) / ln(10);
}

double ln(double x)
{
    double out;
    __asm__ __volatile__("fldln2; fldl %1; fyl2x"
                         : "=t"(out)
                         : "m"(x));
    return out;
}

double logx(double x, double y)
{
    // Base may not equal 1 or be negative.
    if (y == 1.f || y < 0.f || ln(y) == 0.f)
        return 0.f;
    return ln(x) / ln(y);
}

/// Max power for forward and reverse projections.
#define MAXPOWTWO 4.503599627370496000E+15

double modf(double x, double *intpart)
{
    register double absvalue;
    if ((absvalue = (x >= 0.0) ? x : -x) >= MAXPOWTWO) {
        // It must be an integer.
        (*intpart) = x;
    } else {
        // Shift fraction off right.
        (*intpart) = absvalue + MAXPOWTWO;
        // Shift back without fraction.
        (*intpart) -= MAXPOWTWO;

        // Above arithmetic might round.
        while ((*intpart) > absvalue) {
            // Test again just to be sure.
            (*intpart) -= 1.0;
        }
        if (x < 0.0) {
            (*intpart) = -(*intpart);
        }
    }
    // Signed fractional part.
    return (x - (*intpart));
}
