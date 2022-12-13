/// @file ioctl.h
/// @brief Declares device controlling operations.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

/// @brief Manipulates the underlying device parameters of special files, or operating
/// 		characteristics of character special files (e.g., terminals).
/// @param fd      Must be an open file descriptor.
/// @param request The device-dependent request code
/// @param data    An untyped pointer to memory.
/// @return On success zero is returned.
int sys_ioctl(int fd, int request, void *data);
