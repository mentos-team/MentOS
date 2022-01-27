/// @file link_access.h
/// @brief Set of macros that provide access to linking symbols.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#ifdef __APPLE__
#include <mach-o/getsect.h>

/// @brief Define variables pointing to external symbols, specifically
///        to the .data section of a linked object file.
#define EXTLD(NAME) extern const unsigned char _section$__DATA__##NAME[];
/// Provide access to the .data of the linked object.
#define LDVAR(NAME) _section$__DATA__##NAME
/// Provides access to the length of the .data section of the linked object.
#define LDLEN(NAME) (getsectbyname("__DATA", "__" #NAME)->size)

#elif (defined __WIN32__) // mingw

/// @brief Define variables pointing to external symbols, specifically
///        to the .data section of a linked object file.
#define EXTLD(NAME)                                     \
    extern const unsigned char binary_##NAME##_start[]; \
    extern const unsigned char binary_##NAME##_end[];   \
    extern const unsigned char binary_##NAME##_size[];
/// Provide access to the .data of the linked object.
#define LDVAR(NAME) binary_##NAME##_start
/// Provides access to the length of the .data section of the linked object.
#define LDLEN(NAME) binary_##NAME##_size

#else // gnu/linux ld

/// @brief Define variables pointing to external symbols, specifically
///        to the .data section of a linked object file.
#define EXTLD(NAME)                                      \
    extern const unsigned char _binary_##NAME##_start[]; \
    extern const unsigned char _binary_##NAME##_end[];   \
    extern const unsigned char _binary_##NAME##_size[];
/// Provides access to the .data of the linked object.
#define LDVAR(NAME) _binary_##NAME##_start
/// Provides access to the length of the .data section of the linked object.
#define LDLEN(NAME) _binary_##NAME##_size
#endif
