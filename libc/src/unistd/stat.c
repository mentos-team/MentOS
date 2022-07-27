/// @file stat.c
/// @brief Stat functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "system/syscall_types.h"
#include "sys/errno.h"

#include "sys/stat.h"

_syscall2(int, stat, const char *, path, stat_t *, buf)

_syscall2(int, fstat, int, fd, stat_t *, buf)
