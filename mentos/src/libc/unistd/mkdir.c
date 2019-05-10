///                MentOS, The Mentoring Operating system project
/// @file mkdir.c
/// @brief Make directory functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "syscall.h"
#include "unistd.h"
#include "errno.h"
#include "stat.h"

int mkdir(const char *path, mode_t mode)
{
	ssize_t retval;
	DEFN_SYSCALL2(retval, __NR_mkdir, path, mode);
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}
