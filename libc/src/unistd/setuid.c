/// @file setuid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

// _syscall1(int, setuid, uid_t, pid)
int setuid(uid_t pid)
{
    long __res;
    __inline_syscall_1(__res, setuid, pid);
    __syscall_return(int, __res);
}

// _syscall2(int, setreuid, uid_t, ruid, uid_t, euid)
int setreuid(uid_t ruid, uid_t euid)
{
    long __res;
    __inline_syscall_2(__res, setreuid, ruid, euid);
    __syscall_return(int, __res);
}
