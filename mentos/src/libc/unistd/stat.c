///                MentOS, The Mentoring Operating system project
/// @file stat.c
/// @brief Stat functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "syscall.h"
#include "unistd.h"
#include "errno.h"
#include "stat.h"

int stat(const char *path, stat_t *buf)
{
	ssize_t retval;
	DEFN_SYSCALL2(retval, __NR_stat, path, buf);
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}
