/// @file read.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

// _syscall3(ssize_t, read, int, fd, void *, buf, size_t, nbytes)
ssize_t read(int fd, void *buf, size_t nbytes)
{
    long __res;
    __inline_syscall_3(__res, read, fd, buf, nbytes);
    __syscall_return(ssize_t, __res);
}
