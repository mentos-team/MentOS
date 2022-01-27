/// @file ps2.h
/// @brief PS/2 drivers.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Initializes ps2 devices.
/// @return 0 on success, 1 on failure.
int ps2_initialize();

void ps2_write(unsigned char data);

unsigned char ps2_read();