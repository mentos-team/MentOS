/// @file spinlock.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "klib/spinlock.h"

void spinlock_init(spinlock_t *spinlock) { (*spinlock) = SPINLOCK_FREE; }

void spinlock_lock(spinlock_t *spinlock)
{
    while (1) {
        if (atomic_set_and_test(spinlock, SPINLOCK_BUSY) == 0) {
            break;
        }
        // CRITICAL: Use volatile read to prevent compiler from optimizing away
        // the loop. In Release mode, the compiler might eliminate the while loop
        // if it doesn't see that *spinlock changes inside the loop.
        // This causes deadlock when waiting for another CPU to release the lock.
        while (*(volatile spinlock_t *)spinlock) {
            cpu_relax();
        }
    }
}

void spinlock_unlock(spinlock_t *spinlock)
{
    barrier();
    atomic_set(spinlock, SPINLOCK_FREE);
}

int spinlock_trylock(spinlock_t *spinlock) { return atomic_set_and_test(spinlock, SPINLOCK_BUSY) == 0; }
