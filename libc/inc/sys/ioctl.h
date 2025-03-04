/// @file ioctl.h
/// @brief Input/Output ConTroL (IOCTL) functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Executes a device-specific control operation on a file descriptor.
/// @param fd The file descriptor for the device or file being operated on.
/// @param request The `ioctl` command, defining the action or configuration.
/// @param data Additional data needed for the `ioctl` command, often a pointer to user-provided data.
/// @return Returns 0 on success; on error, returns a negative error code.
long ioctl(int fd, unsigned int request, unsigned long data);
