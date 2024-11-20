/// @file getpid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

// _syscall0(pid_t, getpid)
pid_t getpid(void)
{
    long __res;
    __inline_syscall_0(__res, getpid);
    __syscall_return(pid_t, __res);
}
