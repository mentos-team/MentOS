///                MentOS, The Mentoring Operating system project
/// @file   keyboard.h
/// @brief  Definitions about the keyboard.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "kernel.h"

/// @brief The dimension of the circular buffer used to store video history.
#define BUFSIZE 256

/// @brief Function used to install the keyboard.
void keyboard_install();

/// @brief The interrupt service routine of the keyboard.
/// @param f The interrupt stack frame.
void keyboard_isr(pt_regs *f);

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
