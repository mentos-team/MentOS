///                MentOS, The Mentoring Operating system project
/// @file close.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "syscall.h"

int close(int fd)
{
	int retval;
	DEFN_SYSCALL1(retval, __NR_close, fd);
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}
