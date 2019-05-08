///                MentOS, The Mentoring Operating system project
/// @file read_write.c
/// @brief Read and write functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// @brief        Read data from a file descriptor.
/// @param fd     The file descriptor.
/// @param buf    The buffer.
/// @param nbytes The number of bytes to read.
/// @return       The number of read characters.
ssize_t sys_read(int fd, void *buf, size_t nbytes);

/// @brief        Write data into a file descriptor.
/// @param fd     The file descriptor.
/// @param buf    The buffer collecting data to written.
/// @param nbytes The number of bytes to write.
/// @return       The number of written bytes.
ssize_t sys_write(int fd, void *buf, size_t nbytes);
