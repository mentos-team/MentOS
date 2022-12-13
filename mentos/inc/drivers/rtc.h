/// @file rtc.h
/// @brief Real Time Clock (RTC) driver.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup drivers Device Drivers
/// @brief Routines for interfacing with peripheral devices.
/// @{
/// @addtogroup rtc Real Time Clock (RTC)
/// @brief Routines for interfacing with the real-time clock.
/// @{

#pragma once

#include "time.h"

/// @brief Copies the global time inside the provided variable.
/// @param time Pointer where we store the global time.
extern void gettime(tm_t *time);

/// @brief Initializes the Real Time Clock (RTC).
/// @return 0 on success, 1 on error.
int rtc_initialize();

/// @brief De-initializes the Real Time Clock (RTC).
/// @return 0 on success, 1 on error.
int rtc_finalize();

/// @}
/// @}
