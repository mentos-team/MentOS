/// @file getdents.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "sys/errno.h"
#include "system/syscall_types.h"


_syscall3(ssize_t, getdents, int, fd, dirent_t *, dirp, unsigned int, count)
