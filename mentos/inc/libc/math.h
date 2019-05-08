///                MentOS, The Mentoring Operating system project
/// @file math.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief The absolute value.
#define abs(a) (((a) < 0) ? -(a) : (a))

/// @brief The max of the two values.
#define max(a, b) (((a) > (b)) ? (a) : (b))

/// @brief The min of the two values.
#define min(a, b) (((a) < (b)) ? (a) : (b))

/// @brief The sign the the passed value.
#define sign(x) ((x < 0) ? -1 : ((x > 0) ? 1 : 0))

/// @brief Returns a rounded up, away from zero, to the nearest multiple of b.
#define ceil(NUMBER, BASE) (((NUMBER) + (BASE)-1) & ~((BASE)-1))

/// @brief e
#define M_E 2.7182818284590452354

/// @brief log_2 e
#define M_LOG2E 1.4426950408889634074

/// @brief log_10 e
#define M_LOG10E 0.43429448190325182765

/// @brief log_e 2
#define M_LN2 0.69314718055994530942

/// @brief log_e 10
#define M_LN10 2.30258509299404568402

/// @brief pi
#define M_PI 3.14159265358979323846

/// @brief pi / 2
#define M_PI_2 1.57079632679489661923

/// @brief pi / 4
#define M_PI_4 0.78539816339744830962

/// @brief 1 / pi
#define M_1_PI 0.31830988618379067154

/// @brief 2 / pi
#define M_2_PI 0.63661977236758134308

/// @brief 2 / sqrt(pi)
#define M_2_SQRTPI 1.12837916709551257390

/// @brief sqrt(2)
#define M_SQRT2 1.41421356237309504880

/// @brief  1 / sqrt(2)
#define M_SQRT1_2 0.70710678118654752440

// TODO: doxygen comment.
/// @brief
/// @param x
/// @result
double round(double x);

// TODO: doxygen comment.
/// @brief
/// @param x
/// @result
double floor(double x);

/// @brief   Power function.
/// @param x First number.
/// @param y Second number.
/// @result  Power between number x and y.
double pow(double x, double y);

// TODO: doxygen comment.
/// @brief
/// @param base
/// @param value
/// @result
long find_nearest_pow_greater(double base, double value);

/// @brief   Exponential function.
/// @param x Value of the exponent.
double exp(double x);

// TODO: doxygen comment.
/// @brief
/// @param x
/// @result
double fabs(double x);

/// @brief   Square root function.
/// @param x Topic of the square root.
double sqrt(double x);

// TODO: doxygen comment.
/// @brief
/// @param x
/// @result
int isinf(double x);

// TODO: doxygen comment.
/// @brief
/// @param x
/// @result
int isnan(double x);

/// @brief   Logarithm function in base 10.
/// @param x Topic of the logarithm function.
double log10(double x);

/// @brief   Natural logarithm function.
/// @param x Topic of the logarithm function.
double ln(double x);

/// @brief   Logarithm function in base x.
/// @brief x Base of the logarithm.
/// @param y Topic of the logarithm function.
double logx(double x, double y);

/// @brief Breaks x into an integral and a fractional part.
///        The integer part is stored in the object pointed by intpart, and the
///        fractional part is returned by the function. Both parts have the same
///        sign as x.
double modf(double x, double *intpart);
