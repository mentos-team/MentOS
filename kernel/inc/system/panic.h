/// @file panic.h
/// @brief Functions used to manage kernel panic.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// Debug exit port for QEMU's isa-debug-exit device (default iobase=0x501).
/// The host exit code is `(guest_value << 1) | 1`, so 0x10 reaches the host
/// as 33 and 0x11 as 35. Both the panic path and the kernel-test boot mode
/// write to it, which is why the port lives here and not in one of them.
#define DEBUG_EXIT_PORT    0x501
/// Value meaning the run reached its end with nothing to report (host 33).
#define DEBUG_EXIT_SUCCESS 0x10
/// Value meaning the run died (host 35).
#define DEBUG_EXIT_FAILURE 0x11

/// @brief Prints the given message and safely stop the execution of the kernel.
/// @param msg The message that has to be shown.
void kernel_panic(const char *msg);

/// @brief Sends a kernel panic with the given message.
#define TODO(msg) kernel_panic(#msg);
