/// @file utsname.c
/// @brief Functions used to provide information about the machine & OS.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/utsname.h"
#include "system/syscall_types.h"
#include "sys/errno.h"
#include "stddef.h"

_syscall1(int, uname, utsname_t *, buf)
