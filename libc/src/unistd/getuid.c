/// @file getuid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "sys/errno.h"
#include "system/syscall_types.h"

_syscall0(uid_t, getuid)
_syscall0(uid_t, geteuid)
