/// @file fhs.h
/// @brief Filesystem Hierarchy Standard (FHS) initialization.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Initialize standard Filesystem Hierarchy Standard (FHS) directories.
/// @details This function creates the essential system directories that are
/// expected to exist on all Linux-like systems according to FHS 3.0.
/// This includes directories like /tmp, /home, /var, /usr, /etc, etc.
///
/// This function is called during early system initialization after the root
/// filesystem has been mounted.
///
/// @return 0 on success, 0 even if some non-critical directories fail to be created.
int fhs_initialize(void);
