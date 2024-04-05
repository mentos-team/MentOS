/// @file   chmod.c
/// @brief
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "sys/errno.h"
#include "system/syscall_types.h"

_syscall2(int, chmod, const char *, pathname, mode_t, mode)

_syscall2(int, fchmod, int, fd, mode_t, mode)
