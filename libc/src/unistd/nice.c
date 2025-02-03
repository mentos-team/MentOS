/// @file nice.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

// _syscall1(int, nice, int, inc)
int nice(int inc)
{
    long __res;
    __inline_syscall_1(__res, nice, inc);
    __syscall_return(int, __res);
}
