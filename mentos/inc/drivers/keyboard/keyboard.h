/// @file keyboard.h
/// @brief Drivers for the Keyboard devices.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup drivers Device Drivers
/// @{
/// @addtogroup keyboard Keyboard
/// @brief Drivers for the Keyboard devices.
/// @{

#pragma once

#include "ring_buffer.h"
#include "kernel.h"

DECLARE_FIXED_SIZE_RING_BUFFER(int, scancode, 256, -1)

/// @brief The interrupt service routine of the keyboard.
/// @param f The interrupt stack frame.
void keyboard_isr(pt_regs *f);

/// @brief Enable the keyboard.
void keyboard_enable();

/// @brief Disable the keyboard.
void keyboard_disable();

/// @brief Leds handler.
void keyboard_update_leds();

/// @brief Gets and removes a char from the back of the buffer.
/// @return The extracted character.
int keyboard_pop_back();

/// @brief Gets a char from the back of the buffer.
/// @return The read character.
int keyboard_back();

/// @brief Gets a char from the front of the buffer.
/// @return The read character.
int keyboard_front();

/// @brief Initializes the keyboard drivers.
/// @return 0 on success, 1 on error.
int keyboard_initialize();

/// @brief De-initializes the keyboard drivers.
/// @return 0 on success, 1 on error.
int keyboard_finalize();

/// @}
/// @}
