/// @file mutex.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "klib/mutex.h"
#include "io/debug.h"

void mutex_lock(mutex_t *mutex, uint32_t owner)
{
    pr_debug("[%d] Trying to lock mutex...\n", owner);
    int failure = 1;

    while (mutex->state == 0 || failure || mutex->owner != owner) {
        failure = 1;
        if (mutex->state == 0) {
            __asm__ __volatile__("movl $0x01,%%eax\n\t" // move 1 to eax
                                 "xchg    %%eax,%0\n\t" // try to set the lock bit
                                 "mov     %%eax,%1\n\t" // export our result to a test var
                                 : "=m"(mutex->state), "=r"(failure)
                                 : "m"(mutex->state)
                                 : "%eax");
        }
        if (failure == 0) {
            mutex->owner = owner; //test to see if we got the lock bit
        }
    }
}

void mutex_unlock(mutex_t *mutex)
{
    mutex->state = 0;
}
