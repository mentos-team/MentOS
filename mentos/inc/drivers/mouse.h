/// @file mouse.h
/// @brief Driver for *PS2* Mouses.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup drivers Device Drivers
/// @{
/// @addtogroup mouse Mouse
/// @brief Routines for interfacing with the mouse.
/// @{

#pragma once

/* The mouse starts sending automatic packets when the mouse moves or is
 * clicked.
 */
#include "kernel.h"

/// @brief Initializes the mouse.
/// @return 0 on success, 1 on error.
int mouse_initialize();

/// @brief De-initializes the mouse.
/// @return 0 on success, 1 on error.
int mouse_finalize();

/// @}
/// @}
