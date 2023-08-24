/// @file   getcwd.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "sys/errno.h"
#include "system/syscall_types.h"

_syscall2(char *, getcwd, char *, buf, size_t, size)
