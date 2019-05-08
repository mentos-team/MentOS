///                MentOS, The Mentoring Operating system project
/// @file math.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "math.h"
#include <libc/stdint.h>

double round(double x)
{
	double out;
	__asm__("fldln2; fldl %1; frndint" : "=t"(out) : "m"(x));

	return out;
}

double floor(double x)
{
	if (x > -1.0 && x < 1.0) {
		if (x >= 0) {
			return 0.0;
		} else {
			return -1.0;
		}
	}

	int x_i = (int)x;

	if (x < 0) {
		return (double)(x_i - 1);
	} else {
		return (double)x_i;
	}
}

#if 0
double pow(double base, double ex)
{
    // Power of 0.
    if (ex == 0)
    {
        return 1;
    }
    // Negative exponenet.
    else if (ex < 0)
    {
        return 1 / pow(base, -ex);
    }
    // Even exponenet.
    else if ((int) ex % 2 == 0)
    {
        float half_pow = pow(base, ex / 2);

        return half_pow * half_pow;
    }
    // Integer exponenet.
    else
    {
        return base * pow(base, ex - 1);
    }
}
#else

double pow(double x, double y)
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
						 : "0"(x), "u"(y)
						 : "st(1)");
	return out;
}

#endif

long find_nearest_pow_greater(double base, double value)
{
	if (base <= 1) {
		return -1;
	}

	long pow_value = 0;

	while (pow(base, pow_value) < value) {
		pow_value++;
	}

	return pow_value;
}

double exp(double x)
{
	return pow(M_E, x);
}

double fabs(double x)
{
	double out;
	__asm__("fldln2; fldl %1; fabs" : "=t"(out) : "m"(x));

	return out;
}

double sqrt(double x)
{
	double out;
	__asm__("fldln2; fldl %1; fsqrt" : "=t"(out) : "m"(x));

	return out;
}

int isinf(double x)
{
	union {
		unsigned long long u;
		double f;
	} ieee754;
	ieee754.f = x;

	return ((unsigned)(ieee754.u >> 32) & 0x7fffffff) == 0x7ff00000 &&
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
	__asm__("fldln2; fldl %1; fyl2x" : "=t"(out) : "m"(x));

	return out;
}

double logx(double x, double y)
{
	// Base may not equal 1 or be negative.
	if (y == 1.f || y < 0.f || ln(y) == 0.f) {
		return 0.f;
	}

	return ln(x) / ln(y);
}

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
