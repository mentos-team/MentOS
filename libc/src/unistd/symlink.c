/// @file symlink.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

// _syscall2(int, symlink, const char *, linkname, const char *, path)
int symlink(const char *linkname, const char *path)
{
    long __res;
    __inline_syscall_2(__res, symlink, linkname, path);
    __syscall_return(int, __res);
}
