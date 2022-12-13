/// @file fdc.h
/// @brief Definitions about the floppy.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup drivers Device Drivers
/// @{
/// @addtogroup fdc Floppy Disc Controller (FDC)
/// @brief Routines for interfacing with the floppy disc controller.
/// @{

#pragma once

/// @brief Initializes the floppy disk controller.
/// @return 0 on success, 1 on error.
int fdc_initialize();

/// @brief De-initializes the floppy disk controller.
/// @return 0 on success, 1 on error.
int fdc_finalize();

/// @}
/// @}
