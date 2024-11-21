/// @file stdint.h
/// @brief Standard integer data-types.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Define the signed 64-bit integer.
typedef int int64_t;

/// @brief Define the unsigned 64-bit integer.
typedef unsigned int uint64_t;

/// @brief Define the signed 32-bit integer.
typedef int int32_t;

/// @brief Define the unsigned 32-bit integer.
typedef unsigned int uint32_t;

/// @brief Define the signed 16-bit integer.
typedef short int16_t;

/// @brief Define the unsigned 16-bit integer.
typedef unsigned short uint16_t;

/// @brief Define the signed 8-bit integer.
typedef char int8_t;

/// @brief Define the unsigned 8-bit integer.
typedef unsigned char uint8_t;

/// @brief Define the signed 32-bit pointer.
typedef signed intptr_t;

/// @brief Define the unsigned 32-bit pointer.
typedef unsigned uintptr_t;

/// @brief Minimum value of a signed 8-bit integer.
#define INT8_MIN (-128)

/// @brief Minimum value of a signed 16-bit integer.
#define INT16_MIN (-32768)

/// @brief Minimum value of a signed 32-bit integer.
#define INT32_MIN (-2147483648)

/// @brief Maximum value of a signed 8-bit integer.
#define INT8_MAX (+127)

/// @brief Maximum value of a signed 16-bit integer.
#define INT16_MAX (+32767)

/// @brief Maximum value of a signed 32-bit integer.
#define INT32_MAX (+2147483647)

/// @brief Maximum value of an unsigned 8-bit integer.
#define UINT8_MAX (+255)

/// @brief Maximum value of an unsigned 16-bit integer.
#define UINT16_MAX (+65535)

/// @brief Maximum value of an unsigned 32-bit integer.
#define UINT32_MAX (+4294967295U)

/// @brief Maximum value representable by size_t.
#define SIZE_MAX (+4294967295U)
