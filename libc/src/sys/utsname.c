/// @file utsname.c
/// @brief Functions used to provide information about the machine & OS.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/utsname.h"
#include "errno.h"
#include "stddef.h"
#include "system/syscall_types.h"

// _syscall1(int, uname, utsname_t *, buf)

int uname(utsname_t *buf)
{
    long __res;
    __inline_syscall_1(__res, uname, buf);
    __syscall_return(int, __res);
}
