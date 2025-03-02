/// @file   chdir.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

// _syscall1(int, chdir, const char *, path)
int chdir(const char *path)
{
    long __res;
    __inline_syscall_1(__res, chdir, path);
    __syscall_return(int, __res);
}

// _syscall1(int, fchdir, int, fd)
int fchdir(int fd)
{
    long __res;
    __inline_syscall_1(__res, fchdir, fd);
    __syscall_return(int, __res);
}
