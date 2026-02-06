/// @file mutex.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "klib/mutex.h"
#include "io/debug.h"

void mutex_lock(mutex_t *mutex, uint32_t owner)
{
    pr_debug("[%d] Trying to lock mutex...\n", owner);
    int failure = 1;

    // CRITICAL: Use volatile read for mutex->state to prevent compiler from
    // optimizing away the loop in Release mode. The while loop must check
    // the current state of the mutex on every iteration.
    while (*(volatile int *)&mutex->state == 0 || failure || *(volatile uint32_t *)&mutex->owner != owner) {
        failure = 1;
        if (*(volatile int *)&mutex->state == 0) {
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

void mutex_unlock(mutex_t *mutex) { mutex->state = 0; }
