/// @file   boot_math.h
/// @brief  Small integer helpers for the bootloader.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

#define boot_min(left, right) (((left) < (right)) ? (left) : (right))
#define boot_max(left, right) (((left) > (right)) ? (left) : (right))

/// Align an address upward to the next multiple of value.
static inline uint32_t boot_align_up(uint32_t address, uint32_t value)
{
    uint32_t remainder = address % value;
    return address + (remainder ? value - remainder : 0);
}

/// Align an address downward to the previous multiple of value.
static inline uint32_t boot_align_down(uint32_t address, uint32_t value)
{
    return address - (address % value);
}
