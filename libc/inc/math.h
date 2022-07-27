/// @file math.h
/// @brief Mathematical constants and functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
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

/// @brief Returns the integral value that is nearest to x, with
///        halfway cases rounded away from zero.
/// @param x Value to round.
/// @result The value of x rounded to the nearest integral
///         (as a floating-point value).
double round(double x);

/// @brief Rounds x upward, returning the smallest integral value
///        that is not less than x.
/// @param x Value to round up.
/// @return The smallest integral value that is not less than x
///         (as a floating-point value).
double ceil(double x);

/// @brief Rounds x downward, returning the largest integral value
///        that is not greater than x.
/// @param x Value to round down.
/// @return The value of x rounded downward (as a floating-point value).
double floor(double x);

/// @brief Returns base raised to the power exponent:
/// @param base     Base value.
/// @param exponent Exponent value.
/// @result The result of raising base to the power exponent.
/// @details
/// If the base is finite negative and the exponent is finite but not an
///  integer value, it causes a domain error.
/// If both base and exponent are zero, it may also cause a domain error
///  on certain implementations.
/// If base is zero and exponent is negative, it may cause a domain error
///  or a pole error (or none, depending on the library implementation).
/// The function may also cause a range error if the result is too great
///  or too small to be represented by a value of the return type.
double pow(double base, double exponent);

/// @brief Returns the base-e exponential function of x, which is e raised
///        to the power x: e^x.
/// @param x Value of the exponent.
/// @return Exponential value of x.
/// @details
/// If the magnitude of the result is too large to be represented by a value
///  of the return type, the function returns HUGE_VAL (or HUGE_VALF or
///  HUGE_VALL) with the proper sign, and an overflow range error occurs and
///  the global variable errno is set to ERANGE.
double exp(double x);

/// @brief Returns the absolute value of x: |x|.
/// @param x Value whose absolute value is returned.
/// @result The absolute value of x.
double fabs(double x);

/// @brief Returns the absolute value of x: |x|.
/// @param x Value whose absolute value is returned.
/// @result The absolute value of x.
float fabsf(float x);

/// @brief Returns the square root of x.
/// @param x Value whose square root is computed.
/// @return Square root of x. If x is negative, the global variable errno
///         is set to EDOM.
double sqrt(double x);

/// @brief Returns the square root of x.
/// @param x Value whose square root is computed.
/// @return Square root of x. If x is negative, the global variable errno
///         is set to EDOM.
float sqrtf(float x);

/// @brief Checks if the input value is Infinite (INF).
/// @param x The value to check.
/// @return 1 if INF, 0 otherwise.
int isinf(double x);

/// @brief Checks if the input value is Not A Number (NAN).
/// @param x The value to check.
/// @return 1 if NAN, 0 otherwise.
int isnan(double x);

/// @brief Logarithm function in base 10.
/// @param x Topic of the logarithm function.
/// @return Return the result.
double log10(double x);

/// @brief Natural logarithm function.
/// @param x Topic of the logarithm function.
/// @return Return the result.
double ln(double x);

/// @brief Logarithm function in base x.
/// @param x Base of the logarithm.
/// @param y Topic of the logarithm function.
/// @return Return the result.
double logx(double x, double y);

/// @brief Breaks x into an integral and a fractional part, both parts have the same sign as x.
/// @param x       The value we want to break.
/// @param intpart Where we store the integer part.
/// @return the fractional part.
double modf(double x, double *intpart);
