/// @file namei.h
/// @brief Headers for path resolution
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

#define REMOVE_TRAILING_SLASH 1 << 0
#define FOLLOW_LINKS          1 << 1
#define CREAT_LAST_COMPONENT  1 << 2

/// @brief Resolve the given path by following all symbolic links.
/// @param path The path to resolve, can include symbolic links.
/// @param buffer The buffer where the resolved absolute path will be stored.
/// @param buflen The size of the buffer, which should be large enough to hold the resolved path.
/// @param flags The flags controlling the behavior of path resolution (e.g., following symlinks, relative/absolute paths).
/// @return 1 on success, negative errno on failure (e.g., -ENOENT if the path is not found).
int resolve_path(const char *path, char *buffer, size_t buflen, int flags);
