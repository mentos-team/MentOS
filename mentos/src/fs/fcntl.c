/// @file fcntl.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "fs/vfs.h"
#include "process/scheduler.h"
#include "system/syscall.h"

long sys_fcntl(int fd, unsigned int request, unsigned long data)
{
    // Get the current task.
    task_struct *task = scheduler_get_current_process();

    // Check the current FD.
    if (fd < 0 || fd >= task->max_fd) {
        return -EMFILE;
    }

    // Get the file descriptor.
    vfs_file_descriptor_t *vfd = &task->fd_list[fd];

    // Verify that the file exists.
    vfs_file_t *file = vfd->file_struct;
    if (file == NULL) {
        return -ENOSYS;
    }

    // Perform the ioctl operation.
    return vfs_fcntl(file, request, data);
}
