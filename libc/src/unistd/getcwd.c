/// @file   getcwd.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

_syscall2(char *, getcwd, char *, buf, size_t, size)
