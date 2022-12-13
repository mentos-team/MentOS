/// @file ioctl.h
/// @brief Input/Output ConTroL (IOCTL) functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Perform the I/O control operation specified by REQUEST on FD.
///   One argument may follow; its presence and type depend on REQUEST.
/// @param fd      Must be an open file descriptor.
/// @param request The device-dependent request code
/// @param data    An untyped pointer to memory.
/// @return Return value depends on REQUEST. Usually -1 indicates error.
int ioctl(int fd, unsigned long int request, void *data);
