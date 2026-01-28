/// @file limits.h
/// @brief OS numeric limits.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// Number of bits in a `char`.
#define CHAR_BIT 8

/// Minimum value a `signed char` can hold.
#define SCHAR_MIN (-128)

/// Maximum value a `signed char` can hold.
#define SCHAR_MAX 127

/// Minimum value a `char` can hold (assuming signed by default).
#define CHAR_MIN SCHAR_MIN

/// Maximum value a `char` can hold.
#define CHAR_MAX SCHAR_MAX

/// Maximum value an `unsigned char` can hold.
#define UCHAR_MAX 255

/// Maximum value a `signed short int` can hold.
#define SHRT_MAX 32767

/// Minimum value a `signed short int` can hold.
#define SHRT_MIN (-SHRT_MAX - 1)

/// Maximum value an `unsigned short int` can hold.
#define USHRT_MAX 65535

/// Maximum value a `signed int` can hold.
#define INT_MAX 2147483647

/// Minimum value a `signed int` can hold.
#define INT_MIN (-INT_MAX - 1)

/// Maximum value an `unsigned int` can hold.
#define UINT_MAX 4294967295U

/// Maximum value a `signed long int` can hold (assuming 32-bit long).
#define LONG_MAX 2147483647L

/// Minimum value a `signed long int` can hold.
#define LONG_MIN (-LONG_MAX - 1L)

/// Maximum value an `unsigned long int` can hold.
#define ULONG_MAX 4294967295UL

/// Maximum number of characters in a file name.
#define NAME_MAX 255

/// Maximum number of characters in a path name.
#define PATH_MAX 4096

/// Maximum length of arguments provided to exec function.
#define ARG_MAX 256

/// Maximum pid number.
#define PID_MAX_LIMIT 32768

/// Maximum number of links to follow during resolving a path.
#define SYMLOOP_MAX 8
