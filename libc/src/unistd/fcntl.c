/// @file fcntl.c
/// @brief Input/Output ConTroL (IOCTL) functions implementation.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/syscall_types.h"
#include "fcntl.h"
#include "errno.h"

_syscall3(long, fcntl, int, fd, unsigned int, request, unsigned long, data)
