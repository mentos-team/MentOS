/// @file getgid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

_syscall0(pid_t, getgid)
_syscall0(gid_t, getegid)
