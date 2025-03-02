/// @file   getcwd.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

// _syscall2(char *, getcwd, char *, buf, size_t, size)
char *getcwd(char *buf, size_t size)
{
    long __res;
    __inline_syscall_2(__res, getcwd, buf, size);
    __syscall_return(char *, __res);
}
