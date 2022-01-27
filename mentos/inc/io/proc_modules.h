/// @file proc_modules.h
/// @brief Contains functions for managing procfs filesystems.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "fs/vfs.h"

/// @brief Initialize the procfs video files.
/// @return 0 on success, 1 on failure.
int procv_module_init();

/// @brief Initialize the procfs system files.
/// @return 0 on success, 1 on failure.
int procs_module_init();
