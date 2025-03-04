/// @file rmdir.c
/// @brief Make directory functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

// _syscall1(int, rmdir, const char *, path)
int rmdir(const char *path)
{
    long __res;
    __inline_syscall_1(__res, rmdir, path);
    __syscall_return(int, __res);
}
