///                MentOS, The Mentoring Operating system project
/// @file port_io.h
/// @brief Byte I/O on ports prototypes.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// @brief      Used for reading from the I/O ports.
/// @param port The input port.
/// @return     The read value.
uint8_t inportb(uint16_t port);

/// @brief      Used for reading 2 bytes from the I/O ports.
/// @param port The input port.
/// @return     The read value.
uint16_t inports(uint16_t port);

void inportsm(uint16_t port, uint8_t *data, unsigned long size);

/// @brief      Used for reading 4 bytes from the I/O ports.
/// @param port The input port.
/// @return     The read value.
uint32_t inportl(uint16_t port);

/// @brief      Use this to write to I/O ports to send bytes to devices.
/// @param port The output port.
/// @param data The data to write.
void outportb(uint16_t port, uint8_t data);

/// @brief      Use this to write to I/O ports to send 2 bytes to devices.
/// @param port The output port.
/// @param data The data to write.
void outports(uint16_t port, uint16_t data);

void outportsm(uint16_t port, uint8_t *data, uint16_t size);

/// @brief      Use this to write to I/O ports to send 4 bytes to devices.
/// @param port The output port.
/// @param data The data to write.
void outportl(uint16_t port, uint32_t data);
