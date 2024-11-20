/// @file readlink.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

// _syscall3(int, readlink, const char *, path, char *, buffer, size_t, bufsize)
int readlink(const char *path, char *buffer, size_t bufsize)
{
    long __res;
    __inline_syscall_3(__res, readlink, path, buffer, bufsize);
    __syscall_return(int, __res);
}
