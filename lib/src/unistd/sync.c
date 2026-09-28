/// @file sync.c
/// @brief Wrappers for the filesystem synchronization system calls.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

int sync(void)
{
    long __res;
    __inline_syscall_0(__res, sync);
    __syscall_return(int, __res);
}

int syncfs(int fd)
{
    long __res;
    __inline_syscall_1(__res, syncfs, fd);
    __syscall_return(int, __res);
}

int sync_file_range(int fd, off_t offset, off_t nbytes, unsigned int flags)
{
    long __res;
    __inline_syscall_4(__res, sync_file_range, fd, offset, nbytes, flags);
    __syscall_return(int, __res);
}
