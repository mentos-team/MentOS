/// @file read_write.c
/// @brief Read and write functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "process/scheduler.h"
#include "fs/vfs_types.h"
#include "system/panic.h"
#include "sys/errno.h"
#include "fcntl.h"
#include "stdio.h"
#include "fs/vfs.h"

ssize_t sys_read(int fd, void *buf, size_t nbytes)
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

    // Perform the read.
    int read = vfs_read(vfd->file_struct, buf, vfd->file_struct->f_pos, nbytes);

    // Update the offset.
    if (read > 0) {
        vfd->file_struct->f_pos += read;
    }
    return read;
}

ssize_t sys_write(int fd, void *buf, size_t nbytes)
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
    if (!(vfd->flags_mask & O_WRONLY)) {
        return -EROFS;
    }

    // Check the file.
    if (vfd->file_struct == NULL) {
        return -ENOSYS;
    }

    // Perform the write.
    int written = vfs_write(vfd->file_struct, buf, vfd->file_struct->f_pos, nbytes);

    // Update the offset.
    if (written > 0) {
        vfd->file_struct->f_pos += written;
    }
    return written;
}

off_t sys_lseek(int fd, off_t offset, int whence)
{
    task_struct *task = scheduler_get_current_process();
    if (fd < 0 || fd >= task->max_fd) {
        return -1;
    }
    // Get the file descriptor.
    vfs_file_descriptor_t *vfd = &task->fd_list[fd];
    // Check the file.
    if (vfd->file_struct == NULL) {
        return -ENOSYS;
    }
    // Perform the lseek.
    return vfs_lseek(vfd->file_struct, offset, whence);
}
