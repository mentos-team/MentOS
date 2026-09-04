/// @file stdint.h
/// @brief Standard integer data-types.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Define the signed 64-bit integer.
typedef long long int64_t;

/// @brief Define the unsigned 64-bit integer.
typedef unsigned long long uint64_t;

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

/// @brief Minimum value of a signed 64-bit integer.
/// @details Written as `-MAX - 1` because `-9223372036854775808` is parsed as
///          the negation of a constant too large for a signed 64-bit type.
#define INT64_MIN (-9223372036854775807LL - 1)

/// @brief Maximum value of a signed 64-bit integer.
#define INT64_MAX (+9223372036854775807LL)

/// @brief Maximum value of an unsigned 64-bit integer.
#define UINT64_MAX (+18446744073709551615ULL)

/// @brief Maximum value representable by size_t.
#define SIZE_MAX (+4294967295U)

// The width of every one of these types is a promise the rest of the tree
// relies on, and breaking it is silent: a cast to a type that turns out to be
// narrower than advertised is perfectly legal, so the compiler cannot warn
// about it. `uint64_t` and `int64_t` were `unsigned int` and `int` for years,
// which left the ELF bounds checks in elf.c unable to detect the overflow they
// were added to catch, truncated ext2 byte offsets past 4 GiB, cut the ATA
// 48-bit sector count to 32 bits, and made `ata_identity_t` 508 bytes instead
// of the 512 the hardware sends (#270). These make the next such change fail
// the build instead.
typedef char int8_width_check[(sizeof(int8_t) == 1) ? 1 : -1];
typedef char uint8_width_check[(sizeof(uint8_t) == 1) ? 1 : -1];
typedef char int16_width_check[(sizeof(int16_t) == 2) ? 1 : -1];
typedef char uint16_width_check[(sizeof(uint16_t) == 2) ? 1 : -1];
typedef char int32_width_check[(sizeof(int32_t) == 4) ? 1 : -1];
typedef char uint32_width_check[(sizeof(uint32_t) == 4) ? 1 : -1];
typedef char int64_width_check[(sizeof(int64_t) == 8) ? 1 : -1];
typedef char uint64_width_check[(sizeof(uint64_t) == 8) ? 1 : -1];
typedef char intptr_width_check[(sizeof(intptr_t) == sizeof(void *)) ? 1 : -1];
typedef char uintptr_width_check[(sizeof(uintptr_t) == sizeof(void *)) ? 1 : -1];
