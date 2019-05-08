///                MentOS, The Mentoring Operating system project
/// @file spinlock.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "spinlock.h"

void spinlock_init(spinlock_t *spinlock)
{
    (*spinlock) = 0;
}

void spinlock_lock(spinlock_t *spinlock)
{
    while (true)
    {
        if (atomic_set_and_test(spinlock, SPINLOCK_BUSY) == 0)
        {
            break;
        }
        while (*spinlock) cpu_relax();
    }
}

void spinlock_unlock(spinlock_t *spinlock)
{
    barrier();

    atomic_set(spinlock, SPINLOCK_FREE);
}

bool_t spinlock_trylock(spinlock_t *spinlock)
{
    return (bool_t) (atomic_set_and_test(spinlock, SPINLOCK_BUSY) == 0);
}
