/// @file   chmod.c
/// @brief
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

// _syscall2(int, chmod, const char *, pathname, mode_t, mode)
int chmod(const char *pathname, mode_t mode)
{
    long __res;
    __inline_syscall_2(__res, chmod, pathname, mode);
    __syscall_return(int, __res);
}

// _syscall2(int, fchmod, int, fd, mode_t, mode)
int fchmod(int fd, mode_t mode)
{
    long __res;
    __inline_syscall_2(__res, fchmod, fd, mode);
    __syscall_return(int, __res);
}
