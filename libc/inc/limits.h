/// @file limits.h
/// @brief OS numeric limits.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// Number of bits in a `char'.
#define CHAR_BIT 8

/// Minimum value a `signed char' can hold.
#define SCHAR_MIN -128

/// Maximum value a `signed char' can hold.
#define SCHAR_MAX +127

/// Minimum value a `signed char' can hold.
#define CHAR_MIN SCHAR_MIN

/// Maximum value a `signed char' can hold.
#define CHAR_MAX SCHAR_MAX

/// Maximum value a `char' can hold.
#define UCHAR_MAX 255

/// Minimum value a `signed short int' can hold.
#define SHRT_MIN (-32768)

/// Maximum value a `signed short int' can hold.
#define SHRT_MAX (+32767)

/// Maximum value a `unsigned short int' can hold.
#define USHRT_MAX 65535

/// Minimum value a `signed int' can hold.
#define INT_MIN (-2147483648)

/// Maximum values a `signed int' can hold.
#define INT_MAX (+2147483647)

/// Maximum value an `unsigned int' can hold.
#define UINT_MAX (+2147483647)

/// Maximum value a `signed long int' can hold.
#define LONG_MIN (-2147483648L)

/// Minimum value a `signed long int' can hold.
#define LONG_MAX (+2147483647L)

/// Maximum number of characters in a file name.
#define NAME_MAX 255

/// Maximum number of characters in a path name.
#define PATH_MAX 4096

/// Maximum length of arguments provided to exec function.
#define ARG_MAX 256

/// Maximum pid number.
#define PID_MAX_LIMIT 32768

/// Maximum number of links to follow during resolving a path
#define SYMLOOP_MAX 8
