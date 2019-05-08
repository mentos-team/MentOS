///                MentOS, The Mentoring Operating system project
/// @file vfork.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "types.h"
#include "errno.h"
#include "syscall.h"
#include <misc/debug.h>
#include <process/process.h>
#include <process/scheduler.h>

pid_t vfork()
{
	pid_t retval = 0;

	DEFN_SYSCALL0(retval, __NR_vfork);

	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}
