/// @file statfs.c
/// @brief Filesystem statistics syscall wrappers.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "sys/statfs.h"
#include "unistd.h"

int statfs(const char *path, statfs_t *buf)
{
    long __res;
    __inline_syscall_2(__res, statfs, path, buf);
    __syscall_return(int, __res);
}

int fstatfs(int fd, statfs_t *buf)
{
    long __res;
    __inline_syscall_2(__res, fstatfs, fd, buf);
    __syscall_return(int, __res);
}
