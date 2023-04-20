/// @file port_io.h
/// @brief Byte I/O on ports prototypes.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// @brief Reads a 8-bit value from the given port.
/// @param port the port we want to read from.
/// @return the value we read.
uint8_t inportb(uint16_t port);

/// @brief Reads a 16-bit value from the given port.
/// @param port the port we want to read from.
/// @return the value we read.
uint16_t inports(uint16_t port);

/// @brief Reads a 32-bit value from the given port.
/// @param port the port we want to read from.
/// @return the value we read.
uint32_t inportl(uint16_t port);

/// @brief Writes a 8-bit value at the given port.
/// @param port the port we want to write to.
/// @param value the value we want to write.
void outportb(uint16_t port, uint8_t value);

/// @brief Writes a 16-bit value at the given port.
/// @param port the port we want to write to.
/// @param value the value we want to write.
void outports(uint16_t port, uint16_t value);

/// @brief Writes a 32-bit value at the given port.
/// @param port the port we want to write to.
/// @param value the value we want to write.
void outportl(uint16_t port, uint32_t value);

/// @brief Reads multiple 16-bit values from the given port.
/// @param port the port we want to read from.
/// @param value the location where we store the values we read.
/// @param size the amount of values we want to read.
void inportsm(uint16_t port, uint8_t *value, unsigned long size);

/// @brief Writes multiple 16-bit values to the given port.
/// @param port the port we want to write to.
/// @param value the location where we get the values we need to write.
/// @param size the amount of values we want to write.
void outportsm(uint16_t port, uint8_t *value, uint16_t size);
