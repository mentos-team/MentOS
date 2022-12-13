/// @file ps2.h
/// @brief PS/2 drivers.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Initializes ps2 devices.
/// @return 0 on success, 1 on failure.
int ps2_initialize();

/// @brief Writes data to the PS/2 port.
/// @param data the data to write.
void ps2_write(unsigned char data);

/// @brief Reads data from the PS/2 port.
/// @return the data coming from the PS/2 port.
unsigned char ps2_read();
