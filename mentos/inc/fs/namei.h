/// @file namei.h
/// @brief Headers for path resolution
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

#define REMOVE_TRAILING_SLASH 1 << 0
#define FOLLOW_LINKS          1 << 1
#define CREAT_LAST_COMPONENT  1 << 2

/// @brief Resolve the path by following all symbolic links.
/// @param path The path to resolve.
/// @param buffer The buffer where the resolved path is stored.
/// @param buflen The size of the provided resolved_path buffer.
/// @param flags The flags controlling how the path is resolved.
/// @return -errno on fail, 1 on success.
int resolve_path(const char *path, char *buffer, size_t buflen, int flags);
