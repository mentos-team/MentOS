/// @file ioctl.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fs/ioctl.h"
#include "process/scheduler.h"
#include "system/printk.h"
#include "stdio.h"
#include "sys/errno.h"
#include "fs/vfs.h"

int sys_ioctl(int fd, int request, void *data)
{
    // Get the current task.
    task_struct *task = scheduler_get_current_process();

    // Check the current FD.
    if (fd < 0 || fd >= task->max_fd) {
        return -EMFILE;
    }

    // Get the file descriptor.
    vfs_file_descriptor_t *vfd = &task->fd_list[fd];

    // Get the file.
    vfs_file_t *file = vfd->file_struct;
    if (file == NULL) {
        return -ENOSYS;
    }

    // Perform the ioctl.
    return vfs_ioctl(file, request, data);
}
