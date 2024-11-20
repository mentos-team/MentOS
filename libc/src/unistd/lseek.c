/// @file open.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

// _syscall3(off_t, lseek, int, fd, off_t, offset, int, whence)
off_t lseek(int fd, off_t offset, int whence)
{
    long __res;
    __inline_syscall_3(__res, lseek, fd, offset, whence);
    __syscall_return(off_t, __res);
}
