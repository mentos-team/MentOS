/// @file mkdir.c
/// @brief Make directory functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "system/syscall_types.h"
#include "sys/errno.h"

_syscall2(int, mkdir, const char *, path, mode_t, mode)
