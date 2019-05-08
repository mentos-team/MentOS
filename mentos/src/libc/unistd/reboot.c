///                MentOS, The Mentoring Operating system project
/// @file reboot.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "syscall.h"

int reboot(int magic1, int magic2, unsigned int cmd, void *arg)
{
	ssize_t retval;

	DEFN_SYSCALL4(retval, __NR_reboot, magic1, magic2, cmd, arg);

	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}

	return retval;
}
