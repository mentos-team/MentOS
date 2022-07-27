/// @file   chdir.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "system/syscall_types.h"
#include "sys/errno.h"

_syscall1(int, chdir, const char *, path)

_syscall1(int, fchdir, int, fd)
