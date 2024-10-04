/// @file readline.h
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stddef.h"

/// @brief Reads a line from the given file descriptor into the buffer.
/// @param fd The file descriptor to read from.
/// @param buffer The buffer where the read line will be stored. Must not be NULL.
/// @param buflen The size of the buffer.
/// @param read_len A pointer to store the length of the read line. Can be NULL if not needed.
/// @return 1 if a newline was found and the line was read successfully, 
///         0 if the end of the file was reached, 
///        -1 if no newline was found and partial data was read.
int readline(int fd, char *buffer, size_t buflen, ssize_t *read_len);
