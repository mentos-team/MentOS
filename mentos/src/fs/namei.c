/// @file namei.c
/// @brief Implementation of functions fcntl() and open().
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fcntl.h"
#include "fs/vfs.h"
#include "io/debug.h"
#include "process/scheduler.h"
#include "sys/errno.h"

int sys_unlink(const char *path)
{
    return vfs_unlink(path);
}

int sys_mkdir(const char *path, mode_t mode)
{
    return vfs_mkdir(path, mode);
}

int sys_rmdir(const char *path)
{
    return vfs_rmdir(path);
}

int sys_creat(const char *path, mode_t mode)
{
    // Get the current task.
    task_struct *task = scheduler_get_current_process();

    // Search for an unused fd.
    int fd = get_unused_fd();
    if (fd < 0)
        return fd;

    // Try to open the file.
    vfs_file_t *file = vfs_creat(path, mode);
    if (file == NULL) {
        return -errno;
    }

    // Set the file descriptor id.
    task->fd_list[fd].file_struct = file;
    task->fd_list[fd].flags_mask = O_WRONLY|O_CREAT|O_TRUNC;

    // Return the file descriptor and increment it.
    return fd;
}

int sys_symlink(const char *linkname, const char *path)
{
    return vfs_symlink(linkname, path);
}

int sys_readlink(const char *path, char *buffer, size_t bufsize)
{
    // Try to open the file.
    vfs_file_t *file = vfs_open(path, O_RDONLY, 0);
    if (file == NULL) {
        return -errno;
    }
    // Read the link.
    ssize_t nbytes = vfs_readlink(file, buffer, bufsize);
    // Close the file.
    vfs_close(file);
    // Return the number of bytes we read.
    return nbytes;
}
