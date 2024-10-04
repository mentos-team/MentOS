/// @file ps2.h
/// @brief PS/2 drivers.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Initializes ps2 devices.
/// @return 0 on success, 1 on failure.
int ps2_initialize(void);

/// @brief Writes a byte of data to the PS/2 device.
/// @details This function waits until the input buffer of the PS/2 controller
/// is empty before sending the specified data byte to the PS/2 data register.
/// @param data The byte of data to be sent to the PS/2 device.
void ps2_write_data(unsigned char data);

/// @brief Sends a command to the PS/2 controller.
/// @details This function waits until the input buffer of the PS/2 controller
/// is empty before writing the specified command to the PS/2 command register.
/// @param command The command byte to be sent to the PS/2 controller.
void ps2_write_command(unsigned char command);

/// @brief Reads a byte of data from the PS/2 device.
/// @details This function waits until data is available in the output buffer of
/// the PS/2 controller. Once data is available, it reads and returns the byte
/// from the PS/2 data register.
/// @return The byte of data read from the PS/2 device.
unsigned char ps2_read_data(void);
