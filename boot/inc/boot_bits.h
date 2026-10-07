/// @file   boot_bits.h
/// @brief  Small bit-mask helpers for the bootloader.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#define boot_bit_set(value, mask)   ((value) | (mask))
#define boot_bit_clear(value, mask) ((value) & ~(mask))
#define boot_bit_test(value, mask)  ((value) & (mask))
