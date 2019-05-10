///                MentOS, The Mentoring Operating system project
/// @file readdir.c
/// @brief Function for accessing directory entries.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "dirent.h"
#include "syscall.h"
#include "stdio.h"
#include "vfs.h"

dirent_t *sys_readdir(DIR *dirp)
{
	if (dirp == NULL) {
		printf("readdir: cannot read directory :"
			   "Directory pointer is not valid\n");

		return NULL;
	}
	if (mountpoint_list[dirp->fd].dir_op.readdir_f == NULL) {
		printf("readdir: cannot read directory '%s':"
			   "No readdir function\n",
			   dirp->path);

		return NULL;
	}
	return mountpoint_list[dirp->fd].dir_op.readdir_f(dirp);
}
