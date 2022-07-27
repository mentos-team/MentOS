/// @file printk.h
/// @brief Functions for managing the kernel messages.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Write formatted output to stdout.
/// @param format Output formatted as for printf.
/// @param ... List of arguments.
/// @return The number of bytes written in syslog.
int sys_syslog(const char *format, ...);
