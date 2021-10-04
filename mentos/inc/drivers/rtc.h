///                MentOS, The Mentoring Operating system project
/// @file   rtc.h
/// @brief  Real Time Clock (RTC) driver.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "time.h"

/// @brief Copies the global time inside the provided variable.
/// @param time Pointer where we store the global time.
extern void gettime(tm_t *time);

/// @brief Installs the Real Time Clock.
extern void rtc_install(void);