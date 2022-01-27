/// @file spinlock.h
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "klib/stdatomic.h"

/// Determines if the spinlock is free.
#define SPINLOCK_FREE 0
/// Determines if the spinlock is busy.
#define SPINLOCK_BUSY 1

/// @brief Spinlock structure.
typedef atomic_t spinlock_t;

/// @brief Initialize the spinlock.
/// @param spinlock The spinlock we initialize.
void spinlock_init(spinlock_t *spinlock);

/// @brief Try to lock the spinlock.
/// @param spinlock The spinlock we lock.
void spinlock_lock(spinlock_t *spinlock);

/// @brief Try to unlock the spinlock.
/// @param spinlock The spinlock we unlock.
void spinlock_unlock(spinlock_t *spinlock);

/// @brief Try to unlock the spinlock.
/// @param spinlock The spinlock we try to block.
/// @return 1 if succeeded, 0 otherwise.
int spinlock_trylock(spinlock_t *spinlock);
