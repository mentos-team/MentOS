/// @file getuid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

// _syscall0(uid_t, getuid)
uid_t getuid(void)
{
    long __res;
    __inline_syscall_0(__res, getuid);
    __syscall_return(uid_t, __res);
}

// _syscall0(uid_t, geteuid)
uid_t geteuid(void)
{
    long __res;
    __inline_syscall_0(__res, geteuid);
    __syscall_return(uid_t, __res);
}
