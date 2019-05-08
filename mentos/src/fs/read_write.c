///                MentOS, The Mentoring Operating system project
/// @file read_write.c
/// @brief Read and write functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <misc/debug.h>
#include "read_write.h"
#include "vfs.h"
#include "stdio.h"
#include "fcntl.h"
#include "unistd.h"
#include "keyboard.h"
#include "video.h"

ssize_t sys_read(int fd, void *buf, size_t nbytes)
{
	if (fd == STDIN_FILENO) {
		*((char *)buf) = keyboard_getc();
		return 1;
	}

	int mp_id = fd_list[fd].mountpoint_id;
	int fs_fd = fd_list[fd].fs_spec_id;

	if (mountpoint_list[mp_id].operations.read_f != NULL) {
		return mountpoint_list[mp_id].operations.read_f(fs_fd, buf, nbytes);
	} else {
		printf("No READ Found for that file system\n");
	}

	return 0;
}

ssize_t sys_write(int fd, void *buf, size_t nbytes)
{
	if ((fd == STDOUT_FILENO) || (fd == STDERR_FILENO)) {
		for (size_t i = 0; (i < nbytes); ++i)
			video_putc(((char *)buf)[i]);
		return nbytes;
	}

	if (fd > MAX_OPEN_FD) {
		//errno = EBADF;
		return -1;
	}

	if (!(fd_list[fd].flags_mask & O_RDWR)) {
		//errno = EROFS;
		return -1;
	}

	mountpoint_t *mp = &mountpoint_list[fd_list[fd].mountpoint_id];
	if (mp == NULL) {
		//errno = ENODEV;
		return -1;
	}
	if (mp->operations.write_f == NULL) {
		//errno = ENOSYS;
		return -1;
	}

	return mp->operations.write_f(fd_list[fd].fs_spec_id, buf, nbytes);
}
