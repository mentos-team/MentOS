/// @file kill.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

// _syscall2(int, kill, pid_t, pid, int, sig)
int kill(pid_t pid, int sig)
{
    long __res;
    __inline_syscall_2(__res, kill, pid, sig);
    __syscall_return(int, __res);
}
