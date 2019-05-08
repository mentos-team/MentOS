///                MentOS, The Mentoring Operating system project
/// @file   keyboard.h
/// @brief  Definitions about the keyboard.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "kernel.h"
#include "stdint.h"
#include "stdbool.h"

/// @brief The dimension of the circular buffer used to store video history.
#define BUFSIZE 256

/// @brief Function used to install the keyboard.
void keyboard_install();

/// @brief The keyboard handler.
void keyboard_isr(pt_regs *r);

/// @brief Enable the keyboard.
void keyboard_enable();

/// @brief Disable the keyboard.
void keyboard_disable();

/// @brief Leds handler.
void keyboard_update_leds();

/// @brief      Get a char from the buffer.
/// @details    It loops until there is something new to read.
/// @return     The read character.
int keyboard_getc();

/// @brief          Set Keyboard echo Shadow.
/// @param value    1 if you want enable shadow, 0 otherwise.
void keyboard_set_shadow(const bool_t value);

/// @brief          Set Keyboard echo Shadow character.
/// @param value    1 if you want enable shadow, 0 otherwise.
void keyboard_set_shadow_character(const char _shadow_character);

/// @brief Get Keyboard Shadow information.
bool_t keyboard_get_shadow();

/// @brief Get ctrl status.
bool_t keyboard_is_ctrl_pressed();

/// @brief Get shift status.
bool_t keyboard_is_shifted();
