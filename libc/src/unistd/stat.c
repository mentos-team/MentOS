/// @file stat.c
/// @brief Stat functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

#include "sys/stat.h"

// _syscall2(int, stat, const char *, path, stat_t *, buf)
int stat(const char *path, stat_t *buf)
{
    long __res;
    __inline_syscall_2(__res, stat, path, buf);
    __syscall_return(int, __res);
}

// _syscall2(int, fstat, int, fd, stat_t *, buf)
int fstat(int fd, stat_t *buf)
{
    long __res;
    __inline_syscall_2(__res, fstat, fd, buf);
    __syscall_return(int, __res);
}
