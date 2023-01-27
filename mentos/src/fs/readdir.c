/// @file readdir.c
/// @brief Function for accessing directory entries.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/dirent.h"
#include "process/scheduler.h"
#include "system/syscall.h"
#include "system/printk.h"
#include "sys/errno.h"
#include "stdio.h"
#include "fs/vfs.h"
#include "string.h"
#include "assert.h"

ssize_t sys_getdents(int fd, dirent_t *dirp, unsigned int count)
{
    if (dirp == NULL) {
        printf("getdents: cannot read directory :"
               "Directory pointer is not valid\n");
        return 0;
    }
    // Get the current process.
    task_struct *current_process = scheduler_get_current_process();
    // Check the current task.
    assert(current_process && "There is no current process!");
    // Check the current FD.
    if ((fd < 0) || (fd >= current_process->max_fd)) {
        return -EMFILE;
    }
    // Get the process-specific file descriptor.
    vfs_file_descriptor_t *process_fd = &current_process->fd_list[fd];
#if 0
    // Check the permissions.
    if (!(current_process->fd_list[fd].flags_mask & O_RDONLY)) {
        return -EROFS;
    }
#endif
    // Get the associated file.
    vfs_file_t *file = process_fd->file_struct;
    if (file == NULL) {
        return -ENOSYS;
    }
    // Perform the read.
    ssize_t actual_read = vfs_getdents(file, dirp, process_fd->file_struct->f_pos, count);
    // Update the offset, only if the value the function returns is positive.
    if (actual_read > 0)
        process_fd->file_struct->f_pos += actual_read;
    return actual_read;
}
