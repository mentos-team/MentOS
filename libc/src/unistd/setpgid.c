/// @file setpgid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

// _syscall2(int, setpgid, pid_t, pid, pid_t, pgid)
int setpgid(pid_t pid, pid_t pgid)
{
    long __res;
    __inline_syscall_2(__res, setpgid, pid, pgid);
    __syscall_return(int, __res);
}
