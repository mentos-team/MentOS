/// @file initrd.h
/// @brief Headers of functions for initrd filesystem.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Initialize the initrd filesystem.
/// @return 0 if fails, 1 if succeed.
int initrd_init_module(void);

/// @brief Clean up the initrd filesystem.
/// @return 0 if fails, 1 if succeed.
int initrd_cleanup_module(void);
