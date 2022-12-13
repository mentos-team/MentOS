/// @file panic.h
/// @brief Functions used to manage kernel panic.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief     Prints the given message and then safely stop the execution of
///            the kernel.
/// @param msg The message that has to be shown.
void kernel_panic(const char *msg);

#define TODO(msg) kernel_panic(#msg);
