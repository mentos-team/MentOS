/// @file open.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

// _syscall3(int, open, const char *, pathname, int, flags, mode_t, mode)
int open(const char *pathname, int flags, mode_t mode)
{
    long __res;
    __inline_syscall_3(__res, open, pathname, flags, mode);
    __syscall_return(int, __res);
}
