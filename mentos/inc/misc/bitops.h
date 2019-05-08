///                MentOS, The Mentoring Operating system project
/// @file bitops.h
/// @brief Bitmasks functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"
#include "stdbool.h"

/// @brief Finds the first bit set in the given irq mask.
int find_first_bit(unsigned short int irq_mask);

/// @brief        Check if the passed value has the given flag set.
/// @param flags  The value to check.
/// @param flag   The flag to search.
/// @return True  If the value contain the flag, <br>
///         False otherwise.
bool_t has_flag(uint32_t flags, uint32_t flag);

/// @brief       Set the passed flag to the value.
/// @param flags The destination value.
/// @param flag  The flag to set.
void set_flag(uint32_t *flags, uint32_t flag);

/// @brief       Clear the passed flag to the value.
/// @param flags The destination value.
/// @param flag  The flag to clear.
void clear_flag(uint32_t *flags, uint32_t flag);
