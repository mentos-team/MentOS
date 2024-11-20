/// @file interval.c
/// @brief Function for setting allarms.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

// _syscall1(unsigned, alarm, int, seconds)
unsigned alarm(int seconds)
{
    long __res;
    __inline_syscall_1(__res, alarm, seconds);
    __syscall_return(unsigned, __res);
}
