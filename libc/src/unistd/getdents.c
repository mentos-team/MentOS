/// @file getdents.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "dirent.h"
#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

// _syscall3(ssize_t, getdents, int, fd, dirent_t *, dirp, unsigned int, count)
ssize_t getdents(int fd, dirent_t *dirp, unsigned int count)
{
    long __res;
    __inline_syscall_3(__res, getdents, fd, dirp, count);
    __syscall_return(ssize_t, __res);
}
