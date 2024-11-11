/// @file printk.h
/// @brief Functions for managing the kernel messages.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Sends a message to the system log using a specified log level.
/// @param type The log level or priority (e.g., LOG_INFO, LOG_ERR).
/// @param buf Pointer to the message buffer to log.
/// @param len The length of the message in bytes.
/// @return The number of bytes written to the system log, or -1 on failure.
int sys_syslog(int type, char *buf, int len);
