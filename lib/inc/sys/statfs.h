/// @file statfs.h
/// @brief Filesystem statistics functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// Prevents the error when including <bits/statfs.h>.
#define __SYS_STATFS_H

#include "bits/statfs.h"

/// @brief Retrieves filesystem statistics for the mounted filesystem containing path.
/// @param path Path to any file or directory on the target filesystem.
/// @param buf Buffer where filesystem statistics are written.
/// @return 0 on success, -1 on failure and errno is set.
int statfs(const char *path, statfs_t *buf);

/// @brief Retrieves filesystem statistics for an open file descriptor.
/// @param fd File descriptor of an open file on the target filesystem.
/// @param buf Buffer where filesystem statistics are written.
/// @return 0 on success, -1 on failure and errno is set.
int fstatfs(int fd, statfs_t *buf);
