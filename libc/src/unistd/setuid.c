/// @file setuid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "sys/errno.h"
#include "system/syscall_types.h"

_syscall1(int, setuid, uid_t, pid)
_syscall2(int, setreuid, uid_t, ruid, uid_t, euid)
