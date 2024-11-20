/// @file dup.c
/// @brief
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "system/syscall_types.h"
#include "errno.h"

// _syscall1(int, dup, int, fd)
int dup(int fd)
{
    long __res;
    __inline_syscall_1(__res, dup, fd);
    __syscall_return(int, __res);
}
