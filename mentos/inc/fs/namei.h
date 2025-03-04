/// @file namei.h
/// @brief Headers for path resolution.
/// @details This header provides definitions and function declarations for
/// resolving paths, including symbolic links and handling different flags that
/// control the behavior of the path resolution process.
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// Flag to remove any trailing slash from the path.
#define REMOVE_TRAILING_SLASH 1 << 0
/// Flag to follow symbolic links during path resolution.
#define FOLLOW_LINKS          1 << 1
/// Flag to create the last component of the path if it doesn't exist.
#define CREAT_LAST_COMPONENT  1 << 2

/// @brief Resolve the given path by following all symbolic links.
///
/// @details This function resolves a file path, following any symbolic links
/// encountered along the way, and returns the absolute path. The behavior can
/// be controlled with flags such as whether to follow symbolic links and how to
/// handle trailing slashes.
///
/// @param path The path to resolve, can include symbolic links.
/// @param buffer The buffer where the resolved absolute path will be stored.
/// @param buflen The size of the buffer, which should be large enough to hold the resolved path.
/// @param flags The flags controlling the behavior of path resolution (e.g., following symlinks, relative/absolute paths).
/// @return 1 on success, negative errno on failure (e.g., -ENOENT if the path is not found).
int resolve_path(const char *path, char *buffer, size_t buflen, int flags);
