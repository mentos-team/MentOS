/// @file read_write.c
/// @brief Read and write functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "fcntl.h"
#include "fs/vfs.h"
#include "fs/vfs_types.h"
#include "mem/paging.h"
#include "process/scheduler.h"
#include "stdio.h"
#include "system/panic.h"

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

    // The buffer is written through the filesystem layer with supervisor
    // rights: a pointer the caller does not own must never reach it, and a
    // read-only page of the caller's must not be written either (#191).
    if (!paging_is_user_range_writable(buf, nbytes)) {
        return -EFAULT;
    }

    // Perform the read.
    int read = vfs_read(vfd->file_struct, buf, vfd->file_struct->f_pos, nbytes);

    // Update the offset.
    if (read > 0) {
        vfd->file_struct->f_pos += read;
    }
    return read;
}

ssize_t sys_write(int fd, const void *buf, size_t nbytes)
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
    if (!bitmask_check(vfd->flags_mask, O_WRONLY | O_RDWR)) {
        return -EROFS;
    }

    // Check the file.
    if (vfd->file_struct == NULL) {
        return -ENOSYS;
    }

    // The buffer is read through the filesystem layer with supervisor
    // rights: a pointer the caller does not own must never reach it, or
    // write() reads wherever it points (#191).
    if (!paging_is_user_range(buf, nbytes)) {
        return -EFAULT;
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
