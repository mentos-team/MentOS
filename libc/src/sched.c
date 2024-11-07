/// @file sched.c
/// @brief Function for managing scheduler.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/syscall_types.h"
#include "sched.h"
#include "errno.h"

_syscall2(int, sched_setparam, pid_t, pid, const sched_param_t *, param)

_syscall2(int, sched_getparam, pid_t, pid, sched_param_t *, param)

_syscall0(int, waitperiod)
