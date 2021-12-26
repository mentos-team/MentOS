/// @file ext2.h
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief EXT2 driver.
/// @version 0.1
/// @date 2021-12-13
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Initializes the EXT2 drivers.
/// @return 0 on success, 1 on error.
int ext2_initialize();

/// @brief De-initializes the EXT2 drivers.
/// @return 0 on success, 1 on error.
int ext2_finalize();
