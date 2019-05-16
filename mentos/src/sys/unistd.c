///                MentOS, The Mentoring Operating system project
/// @file unistd.c
/// @brief Functions used to manage files.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"

#include "string.h"
#include "limits.h"
#include "stdio.h"
#include "vfs.h"

int rmdir(const char *path)
{
	char absolute_path[PATH_MAX];
	strcpy(absolute_path, path);

	if (path[0] != '/') {
		get_absolute_path(absolute_path);
	}

	int32_t mp_id = get_mountpoint_id(absolute_path);
	if (mp_id < 0) {
		printf("rmdir: failed to remove '%s':"
			   "Cannot find mount-point\n",
			   path);

		return -1;
	}

	if (mountpoint_list[mp_id].dir_op.rmdir_f == NULL) {
		return -1;
	}

	return mountpoint_list[mp_id].dir_op.rmdir_f(absolute_path);
}
