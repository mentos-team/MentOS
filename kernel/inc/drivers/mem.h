/// @file mem.h
/// @brief Drivers for memory devices
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup drivers Device Drivers
/// @{
/// @addtogroup mem Memory devices
/// @brief Drivers for memory devices.
/// @{

#pragma once

/// @brief Initializes memory devices and registers the null device.
///
/// @details This function creates the `/dev/null` device, registers the null
/// filesystem type, and mounts the null device to the virtual file system. If
/// any step fails, it logs the error and returns an appropriate error code.
///
/// @return 0 on success, -ENODEV if device creation fails, 1 if filesystem
/// registration or mounting fails.
int mem_devs_initialize(void);

/// @}
/// @}
