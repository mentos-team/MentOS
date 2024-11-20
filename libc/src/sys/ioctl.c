/// @file ioctl.c
/// @brief Input/Output ConTroL (IOCTL) functions implementation.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/ioctl.h"
#include "errno.h"
#include "system/syscall_types.h"

// _syscall3(long, ioctl, int, fd, unsigned int, request, unsigned long, data)
long ioctl(int fd, unsigned int request, unsigned long data)
{
    long __res;
    __inline_syscall_3(__res, ioctl, fd, request, data);
    __syscall_return(long, __res);
}
