/// @file reboot.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "system/syscall_types.h"
#include "unistd.h"

// _syscall4(int, reboot, int, magic1, int, magic2, unsigned int, cmd, void *, arg)
int reboot(int magic1, int magic2, unsigned int cmd, void *arg)
{
    long __res;
    __inline_syscall_4(__res, reboot, magic1, magic2, cmd, arg);
    __syscall_return(int, __res);
}
