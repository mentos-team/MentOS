/// @file mm_io.h
/// @brief Memory Mapped IO functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// @brief Reads a 8-bit value from the given address.
/// @param addr the address we want to read from.
/// @return the value we read.
uint8_t in_memb(uint32_t addr);

/// @brief Reads a 16-bit value from the given address.
/// @param addr the address we want to read from.
/// @return the value we read.
uint16_t in_mems(uint32_t addr);

/// @brief Reads a 32-bit value from the given address.
/// @param addr the address we want to read from.
/// @return the value we read.
uint32_t in_meml(uint32_t addr);

/// @brief Writes a 8-bit value at the given address.
/// @param addr the address we want to write to.
/// @param value the value we want to write.
void out_memb(uint32_t addr, uint8_t value);

/// @brief Writes a 16-bit value at the given address.
/// @param addr the address we want to write to.
/// @param value the value we want to write.
void out_mems(uint32_t addr, uint16_t value);

/// @brief Writes a 32-bit value at the given address.
/// @param addr the address we want to write to.
/// @param value the value we want to write.
void out_meml(uint32_t addr, uint32_t value);
