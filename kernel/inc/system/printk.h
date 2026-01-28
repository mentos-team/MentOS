/// @file printk.h
/// @brief Functions for managing the kernel messages.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Sends a message to the system log using a specified log level.
/// @param file the name of the file.
/// @param fun the name of the function.
/// @param line the line inside the file.
/// @param log_level the log level.
/// @param format the format to used, see printf.
void sys_syslog(const char *file, const char *fun, int line, short log_level, const char *format);
