/// @file reboot.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "system/syscall_types.h"
#include "sys/errno.h"

_syscall4(int, reboot, int, magic1, int, magic2, unsigned int, cmd, void *, arg)
