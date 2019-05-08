///                MentOS, The Mentoring Operating system project
/// @file spinlock.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "debug.h"
#include "stddef.h"
#include "irqflags.h"
#include "stdatomic.h"
#include "scheduler.h"

#define SPINLOCK_FREE 0

#define SPINLOCK_BUSY 1

/// @brief Spinlock structure.
typedef atomic_t spinlock_t;

/// @brief Initialize the spinlock.
void spinlock_init(spinlock_t *spinlock);

/// @brief Try to lock the spinlock.
void spinlock_lock(spinlock_t *spinlock);

/// @brief Try to unlock the spinlock.
void spinlock_unlock(spinlock_t *spinlock);

/// @brief Try to unlock the spinlock.
bool_t spinlock_trylock(spinlock_t *spinlock);
