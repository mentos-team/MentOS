/// @file getppid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

// _syscall0(pid_t, getppid)
pid_t getppid(void)
{
    long __res;
    __inline_syscall_0(__res, getppid);
    __syscall_return(pid_t, __res);
}
