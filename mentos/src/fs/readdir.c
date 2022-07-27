/// @file readdir.c
/// @brief Function for accessing directory entries.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/dirent.h"
#include "process/scheduler.h"
#include "system/syscall.h"
#include "system/printk.h"
#include "sys/errno.h"
#include "stdio.h"
#include "fs/vfs.h"
#include "string.h"

int sys_getdents(int fd, dirent_t *dirp, unsigned int count)
{
    if (dirp == NULL) {
        printf("getdents: cannot read directory :"
               "Directory pointer is not valid\n");
        return 0;
    }
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
    if (!(task->fd_list[fd].flags_mask & O_RDONLY)) {
        return -EROFS;
    }
#endif

    // Get the file.
    vfs_file_t *file = vfd->file_struct;
    if (file == NULL) {
        return -ENOSYS;
    }
    // Clean the buffer.
    memset(dirp, 0, count);

    // Perform the read.
    int actual_read = vfs_getdents(file, dirp, vfd->file_struct->f_pos, count);

    // Update the offset.
    if (actual_read > 0)
        vfd->file_struct->f_pos += actual_read;
    return actual_read;
}
