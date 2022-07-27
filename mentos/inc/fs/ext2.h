/// @file ext2.h
/// @brief EXT2 driver.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Initializes the EXT2 drivers.
/// @return 0 on success, 1 on error.
int ext2_initialize();

/// @brief De-initializes the EXT2 drivers.
/// @return 0 on success, 1 on error.
int ext2_finalize();
