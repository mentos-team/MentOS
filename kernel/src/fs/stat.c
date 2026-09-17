/// @file stat.c
/// @brief Stat functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "fs/vfs.h"
#include "io/debug.h"
#include "limits.h"
#include "mem/paging.h"
#include "process/scheduler.h"
#include "stdio.h"
#include "string.h"

int sys_stat(const char *path, stat_t *buf)
{
    // Both arguments come from the caller: the path may only be walked
    // once it has proven to live in the caller's memory, and the answer
    // is only written into memory the caller can write (#191).
    if (strnlen_user(path, PATH_MAX) < 0) {
        return -EFAULT;
    }
    if (!paging_is_user_range_writable(buf, sizeof(*buf))) {
        return -EFAULT;
    }
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

    // The answer is written into the caller's memory or nowhere (#191).
    if (!paging_is_user_range_writable(buf, sizeof(*buf))) {
        return -EFAULT;
    }

    return vfs_fstat(vfd->file_struct, buf);
}

int sys_statfs(const char *path, statfs_t *buf)
{
    // Same contract as sys_stat (#191).
    if (strnlen_user(path, PATH_MAX) < 0) {
        return -EFAULT;
    }
    if (!paging_is_user_range_writable(buf, sizeof(*buf))) {
        return -EFAULT;
    }
    return vfs_statfs(path, buf);
}

int sys_fstatfs(int fd, statfs_t *buf)
{
    // Get the current task.
    task_struct *task = scheduler_get_current_process();

    // Check the current FD.
    if (fd < 0 || fd >= task->max_fd) {
        return -EMFILE;
    }

    // Get the file descriptor.
    vfs_file_descriptor_t *vfd = &task->fd_list[fd];

    // Check the file.
    if (vfd->file_struct == NULL) {
        return -ENOSYS;
    }

    // The answer is written into the caller's memory or nowhere (#191).
    if (!paging_is_user_range_writable(buf, sizeof(*buf))) {
        return -EFAULT;
    }

    return vfs_fstatfs(vfd->file_struct, buf);
}
