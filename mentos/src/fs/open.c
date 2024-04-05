/// @file open.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "process/scheduler.h"
#include "fcntl.h"
#include "fs/vfs.h"
#include "io/debug.h"
#include "limits.h"
#include "process/process.h"
#include "stdio.h"
#include "string.h"
#include "sys/errno.h"
#include "system/printk.h"
#include "system/syscall.h"

int sys_open(const char *pathname, int flags, mode_t mode)
{
    // Get the current task.
    task_struct *task = scheduler_get_current_process();

    // Search for an unused fd.
    int fd = get_unused_fd();
    if (fd < 0)
        return fd;

    // Try to open the file.
    vfs_file_t *file = vfs_open(pathname, flags, mode);
    if (file == NULL) {
        return -errno;
    }

    // Set the file descriptor id.
    task->fd_list[fd].file_struct = file;

    if (!bitmask_check(flags, O_APPEND)) {
        // Reset the offset.
        task->fd_list[fd].file_struct->f_pos = 0;
    } else {
        stat_t stat;
        // Stat the file.
        file->fs_operations->stat_f(file, &stat);
        // Point at the last character
        task->fd_list[fd].file_struct->f_pos = stat.st_size;
    }

    // Set the flags.
    task->fd_list[fd].flags_mask = flags;

    // Return the file descriptor and increment it.
    return fd;
}

int sys_close(int fd)
{
    // Get the current task.
    task_struct *task = scheduler_get_current_process();

    // Check the current FD.
    if (fd < 0 || fd >= task->max_fd) {
        return -EMFILE;
    }

    // Get the file.
    vfs_file_t *file = task->fd_list[fd].file_struct;
    if (file == NULL) {
        return -1;
    }

    // Remove the reference to the file.
    task->fd_list[fd].file_struct = NULL;

    // Call the close function.
    return vfs_close(file);
}
