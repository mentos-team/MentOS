/// @file exit.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "system/syscall_types.h"

void exit(int status)
{
    long __res;
    __inline_syscall1(__res, exit, status);
    // The process never returns from this system call!
}
