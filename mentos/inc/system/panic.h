/// @file panic.h
/// @brief Functions used to manage kernel panic.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Prints the given message and safely stop the execution of the kernel.
/// @param msg The message that has to be shown.
void kernel_panic(const char *msg);

/// @brief Sends a kernel panic with the given message.
#define TODO(msg) kernel_panic(#msg);
