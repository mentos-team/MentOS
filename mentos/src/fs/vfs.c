///                MentOS, The Mentoring Operating system project
/// @file vfs.c
/// @brief Headers for Virtual File System (VFS).
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <process/process.h>
#include <libc/stdlib.h>
#include "video.h"
#include "vfs.h"
#include "debug.h"
#include "shell.h"
#include "string.h"
#include "initrd.h"

int current_fd;
char *module_start[MAX_MODULES];
file_descriptor_t fd_list[MAX_OPEN_FD];
mountpoint_t mountpoint_list[MAX_MOUNTPOINT];

void vfs_init()
{
	current_fd = 0;

	for (int32_t i = 0; i < MAX_MOUNTPOINT; ++i) {
		strcpy(mountpoint_list[i].mountpoint, "");
		mountpoint_list[i].mp_id = i;
		mountpoint_list[i].uid = 0;
		mountpoint_list[i].gid = 0;
		mountpoint_list[i].pmask = 0;
		mountpoint_list[i].dev_id = 0;
		mountpoint_list[i].start_address = 0;
		mountpoint_list[i].dir_op.opendir_f = NULL;
		mountpoint_list[i].operations.open_f = NULL;
		mountpoint_list[i].operations.close_f = NULL;
		mountpoint_list[i].operations.read_f = NULL;
		mountpoint_list[i].operations.write_f = NULL;
		mountpoint_list[i].stat_op.stat_f = NULL;
	}

	for (int32_t i = 0; i < MAX_OPEN_FD; ++i) {
		fd_list[i].fs_spec_id = -1;
		fd_list[i].mountpoint_id = -1;
		fd_list[i].offset = 0;
		fd_list[i].flags_mask = 0;
	}

	strcpy(mountpoint_list[0].mountpoint, "/");
	mountpoint_list[0].uid = 0;
	mountpoint_list[0].gid = 0;
	mountpoint_list[0].pmask = 0;
	mountpoint_list[0].dev_id = 0;
	mountpoint_list[0].start_address = (unsigned int)module_start[0];
	mountpoint_list[0].end_address = (unsigned int)module_end[0];
	mountpoint_list[0].dir_op.opendir_f = initfs_opendir;
	mountpoint_list[0].dir_op.closedir_f = initfs_closedir;
	mountpoint_list[0].dir_op.readdir_f = initrd_readdir;
	mountpoint_list[0].dir_op.mkdir_f = initrd_mkdir;
	mountpoint_list[0].dir_op.rmdir_f = initrd_rmdir;
	mountpoint_list[0].operations.open_f = initfs_open;
	mountpoint_list[0].operations.remove_f = initfs_remove;
	mountpoint_list[0].operations.close_f = initrd_close;
	mountpoint_list[0].operations.read_f = initfs_read;
	mountpoint_list[0].operations.write_f = initrd_write;
	mountpoint_list[0].stat_op.stat_f = initrd_stat;
}

int32_t get_mountpoint_id(const char *path)
{
	size_t last_mpl = 0;
	int32_t last_mp_id = -1;

	for (uint32_t it = 0; it < MAX_MOUNTPOINT; ++it) {
		size_t mpl = strlen(mountpoint_list[it].mountpoint);

		if (!strncmp(path, mountpoint_list[it].mountpoint, mpl)) {
			if (((mpl > last_mpl) && it > 0) || (it == 0)) {
				last_mpl = mpl;
				last_mp_id = it;
			}
		}
	}

	return last_mp_id;
}

mountpoint_t *get_mountpoint(const char *path)
{
	int32_t mp_id = get_mountpoint_id(path);

	if (mp_id < 0) {
		return NULL;
	}

	return &mountpoint_list[mp_id];
}

mountpoint_t *get_mountpoint_from_id(int32_t mp_id)
{
	if (mp_id >= MAX_MOUNTPOINT) {
		return NULL;
	}

	return &mountpoint_list[mp_id];
}

int get_relative_path(uint32_t mp_id, char *path)
{
	char relative_path[MAX_PATH_LENGTH];

	// Get the size of the path.
	size_t path_size = strlen(path);

	// Get the size of the mount-point.
	size_t mnpt_size = strlen(mountpoint_list[mp_id].mountpoint);

	// Get the size of the relative path.
	size_t rltv_size = path_size - mnpt_size;

	// Copy the relative path.
	if (rltv_size > 0) {
		size_t i;
		for (i = 0; i < rltv_size; ++i) {
			relative_path[i] = path[mnpt_size + i];
		}
		relative_path[i] = '\0';
		strcpy(path, relative_path);
	}

	return strlen(path);
}

int get_absolute_path(char *path)
{
	if (path[0] != '/') {
		char abspath[MAX_PATH_LENGTH];
		memset(abspath, '\0', MAX_PATH_LENGTH);
		getcwd(abspath, MAX_PATH_LENGTH);
		int abs_size = strlen(abspath);
		if (abspath[abs_size - 1] == '/') {
			strncat(abspath, path, strlen(path));
		} else {
			strncat(abspath, "/", strlen(path));
			strncat(abspath, path, strlen(path));
		}
		strcpy(path, abspath);

		return strlen(path);
	}

	return 0;
}

void vfs_dump()
{
	dbg_print("File Descriptors:\n");
	int i = 0;

	while (i < MAX_OPEN_FD) {
		if (fd_list[i].mountpoint_id != -1) {
			dbg_print("[%d] %s\n", i,
					  mountpoint_list[fd_list[i].mountpoint_id].mountpoint);
		} else {
			dbg_print("[%d] Free\n", i);
		}
		++i;
	}
}
