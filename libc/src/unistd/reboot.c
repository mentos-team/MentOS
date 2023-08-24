/// @file reboot.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "sys/errno.h"
#include "system/syscall_types.h"

_syscall4(int, reboot, int, magic1, int, magic2, unsigned int, cmd, void *, arg)
