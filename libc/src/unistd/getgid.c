/// @file getgid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

// _syscall0(pid_t, getgid)
pid_t getgid(void)
{
    long __res;
    __inline_syscall_0(__res, getgid);
    __syscall_return(pid_t, __res);
}

// _syscall0(gid_t, getegid)
gid_t getegid(void)
{
    long __res;
    __inline_syscall_0(__res, getegid);
    __syscall_return(gid_t, __res);
}
