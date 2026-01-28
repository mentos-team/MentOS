/// @file pid_manager.h
/// @brief Manages the PIDs in the system.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/types.h"

/// @brief Initializes the PID bitmap, marking all PIDs as free.
void pid_manager_init(void);

/// @brief Marks a PID as used in the bitmap.
/// @param pid The PID to mark as used.
void pid_manager_mark_used(pid_t pid);

/// @brief Marks a PID as free in the bitmap.
/// @param pid The PID to mark as free.
void pid_manager_mark_free(pid_t pid);

/// @brief Returns a unique PID.
/// @return a free PID.
pid_t pid_manager_get_free_pid(void);
