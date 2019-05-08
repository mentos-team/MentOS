///                MentOS, The Mentoring Operating system project
/// @file getppid.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "types.h"
#include "syscall.h"

pid_t getppid()
{
	pid_t ret;

	DEFN_SYSCALL0(ret, __NR_getppid);

	return ret;
}
