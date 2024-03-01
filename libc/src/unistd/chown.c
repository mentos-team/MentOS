/// @file   chown.c
/// @brief
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "sys/errno.h"
#include "system/syscall_types.h"

_syscall3(int, chown, const char *, pathname, uid_t, owner, gid_t, group)

_syscall3(int, lchown, const char *, pathname, uid_t, owner, gid_t, group)

_syscall3(int, fchown, int, fd, uid_t, owner, gid_t, group)
