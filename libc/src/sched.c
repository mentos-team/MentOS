/// @file sched.c
/// @brief Function for managing scheduler.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/syscall_types.h"
#include "sched.h"
#include "errno.h"

// _syscall2(int, sched_setparam, pid_t, pid, const sched_param_t *, param)
int sched_setparam(pid_t pid, const sched_param_t *param)
{
    long __res;
    __inline_syscall_2(__res, sched_setparam, pid, param);
    __syscall_return(int, __res);
}

// _syscall2(int, sched_getparam, pid_t, pid, sched_param_t *, param)
int sched_getparam(pid_t pid, sched_param_t *param)
{
    long __res;
    __inline_syscall_2(__res, sched_getparam, pid, param);
    __syscall_return(int, __res);
}

// _syscall0(int, waitperiod)
int waitperiod(void)
{
    long __res;
    __inline_syscall_0(__res, waitperiod);
    __syscall_return(int, __res);
}
