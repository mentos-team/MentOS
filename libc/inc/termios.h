/// @file termios.h
/// @brief Defines the termios functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "bits/termios-struct.h"

/// @brief Put the state of FD into *TERMIOS_P.
/// @param fd
/// @param termios_p
/// @return
extern int tcgetattr(int fd, termios_t *termios_p);

/// @brief Set the state of FD to *TERMIOS_P.
/// @param fd
/// @param optional_actions
/// @param termios_p
/// @return
extern int tcsetattr(int fd, int optional_actions, const termios_t *termios_p);
