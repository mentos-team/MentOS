/// @file ioctl.c
/// @brief Input/Output ConTroL (IOCTL) functions implementation.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/ioctl.h"
#include "sys/errno.h"
#include "system/syscall_types.h"

_syscall3(int, ioctl, int, fd, unsigned long int, request, void *, data)
