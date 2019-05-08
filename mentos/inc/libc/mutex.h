///                MentOS, The Mentoring Operating system project
/// @file mutex.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// @brief Structure of a mutex.
typedef struct mutex_t {
	/// The state of the mutex.
	uint8_t state;
	/// The owner of the mutex.
	uint32_t owner;
} mutex_t;

/// @brief       Allows to lock a mutex.
/// @param mutex The mutex to lock.
/// @param owner The one who request the lock.
void mutex_lock(mutex_t *mutex, uint32_t owner);

/// @brief       Unlocks the mutex.
/// @param mutex The mutex to unlock.
void mutex_unlock(mutex_t *mutex);
