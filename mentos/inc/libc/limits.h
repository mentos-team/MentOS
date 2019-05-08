///                MentOS, The Mentoring Operating system project
/// @file limits.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// Number of bits in a `char'.
#define CHAR_BIT    8

/// Minimum value a `signed char' can hold.

#define SCHAR_MIN    (-128)

/// Maximum value a `signed char' can hold.
#define SCHAR_MAX    127

/// Maximum value a `char' can hold.  (Minimum is 0.)
#define CHAR_MAX    255

/// Minimum value a `signed short int' can hold.
#define SHRT_MIN    (-32768)

/// Maximum value a `signed short int' can hold.
#define SHRT_MAX    32767

/// Minimum value a `signed int' can hold.
#define INT_MIN    (-INT_MAX - 1)

/// Maximum values a `signed int' can hold.
#define INT_MAX    2147483647

/// Maximum value an `unsigned int' can hold.  (Minimum is 0.)
#define UINT_MAX    4294967295U

/// Minimum value a `signed long int' can hold.
#define LONG_MAX    2147483647L

/// Maximum value a `signed long int' can hold.
#define LONG_MIN    (-LONG_MAX - 1L)
