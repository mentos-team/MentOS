/// @file mkdir.c
/// @brief Make directory functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

// _syscall2(int, mkdir, const char *, path, mode_t, mode)
int mkdir(const char *path, mode_t mode)
{
    long __res;
    __inline_syscall_2(__res, mkdir, path, mode);
    __syscall_return(int, __res);
}
