///                MentOS, The Mentoring Operating system project
/// @file fcntl.c
/// @brief Implementation of functions fcntl() and open().
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fcntl.h"
#include "string.h"
#include "vfs.h"
#include "limits.h"

int remove(const char *pathname)
{
	char absolute_path[PATH_MAX];
	strcpy(absolute_path, pathname);
	if (pathname[0] != '/') {
		get_absolute_path(absolute_path);
	}

	int32_t mp_id = get_mountpoint_id(absolute_path);
	if (mp_id < 0) {
		return -1;
	}

	if (mountpoint_list[mp_id].operations.remove_f == NULL) {
		return -1;
	}

	return mountpoint_list[mp_id].operations.remove_f(absolute_path);
}
