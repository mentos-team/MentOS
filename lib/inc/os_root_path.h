/// @file os_root_path.h
/// @brief Provides a macro to extract the relative path of the current file from the project root.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Extracts the relative file path by removing the `MENTOS_ROOT` prefix.
/// @note If `MENTOS_ROOT` does not match the start of `__FILE__`, the full file path is returned.
#ifndef __RELATIVE_PATH__
#define __RELATIVE_PATH__                                                                                              \
    (__builtin_strncmp(__FILE__, MENTOS_ROOT, sizeof(MENTOS_ROOT) - 1) == 0 ? (&__FILE__[sizeof(MENTOS_ROOT)])         \
                                                                            : __FILE__)
#endif
