/// @file readline.h
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stddef.h"

/// @brief Reads a line from the file.
/// @param fd the file descriptor.
/// @param buffer the buffer where we place the line.
/// @param buflen the length of the buffer.
/// @param readlen the amount we read, if negative, we did not encounter a newline.
/// @return 0 if we are done reading, 1 if we encountered a newline, -1 if otherwise.
int readline(int fd, char *buffer, size_t buflen, ssize_t *read_len);
