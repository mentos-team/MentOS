/// @file stat.c
/// @brief Stat functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "io/debug.h"
#include "sys/errno.h"
#include "fs/vfs.h"
#include "mem/kheap.h"
#include "stdio.h"
#include "string.h"
#include "limits.h"

int sys_stat(const char *path, stat_t *buf)
{
    return vfs_stat(path, buf);
}

int sys_fstat(int fd, stat_t *buf)
{
    // Get the current task.
    task_struct *task = scheduler_get_current_process();

    // Check the current FD.
    if (fd < 0 || fd >= task->max_fd) {
        return -EMFILE;
    }

    // Get the file descriptor.
    vfs_file_descriptor_t *vfd = &task->fd_list[fd];

    // Check the permissions.
#if 0
    if (!(vfd->flags_mask & O_RDONLY)) {
        return -EROFS;
    }
#endif

    // Check the file.
    if (vfd->file_struct == NULL) {
        return -ENOSYS;
    }

    return vfs_fstat(vfd->file_struct, buf);
}
