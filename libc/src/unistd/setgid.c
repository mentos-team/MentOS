/// @file setgid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/syscall_types.h"

#include "unistd.h"
#include "errno.h"

// _syscall1(int, setgid, pid_t, gid)
int setgid(gid_t gid)
{
    long __res;
    __inline_syscall_1(__res, setgid, gid);
    __syscall_return(int, __res);
}

// _syscall2(int, setregid, gid_t, rgid, gid_t, egid)
int setregid(gid_t rgid, gid_t egid)
{
    long __res;
    __inline_syscall_2(__res, setregid, rgid, egid);
    __syscall_return(int, __res);
}
