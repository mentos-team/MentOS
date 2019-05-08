///                MentOS, The Mentoring Operating system project
/// @file fcntl.c
/// @brief Implementation of functions fcntl() and open().
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "vfs.h"
#include "debug.h"
#include "kheap.h"
#include "shell.h"
#include "errno.h"
#include "fcntl.h"
#include "string.h"
#include "kernel.h"

int sys_open(const char *pathname, int flags, mode_t mode)
{
    // Allocate a variable for the path.
    char absolute_path[MAX_PATH_LENGTH];

    // Copy the path to the working variable.
    strcpy(absolute_path, pathname);

    // If the first character is not the '/' then get the absolute path.
    if (absolute_path[0] != '/')
    {
        if (!get_absolute_path(absolute_path))
        {
            dbg_print("Cannot get the absolute path.\n");
            return -1;
        }
    }

    // Search for an unused fd.
    for (current_fd = 0; current_fd < MAX_OPEN_FD; ++current_fd)
    {
        if (fd_list[current_fd].mountpoint_id == -1)
        {
            break;
        }
    }

    // Check if there is not fd available.
    if (current_fd == MAX_OPEN_FD)
    {
        //errno = EMFILE;
        return -1;
    }

    // Get the mountpoint.
    mountpoint_t *mp = get_mountpoint(absolute_path);
    if (mp == NULL)
    {
        //errno = ENODEV;
        return -1;
    }

    // Check if the function is implemented.
    if (mp->operations.open_f == NULL)
    {
        //errno = ENOSYS;
        // Reset the file descriptor.
        close(current_fd);
        return -1;
    }

    int32_t fs_spec_id = mp->operations.open_f(absolute_path, flags, mode);
    if (fs_spec_id == -1)
    {
        // Reset the file descriptor.
        close(current_fd);
        return -1;
    }

    // Set the file descriptor id.
    fd_list[current_fd].fs_spec_id = fs_spec_id;

    // Set the mount point id.
    fd_list[current_fd].mountpoint_id = mp->mp_id;

    // Reset the offset.
    fd_list[current_fd].offset = 0;

    // Set the flags.
    fd_list[current_fd].flags_mask = flags;

    // Return the file descriptor and increment it.
    return (current_fd++);
}

int remove(const char *pathname)
{
    char absolute_path[MAX_PATH_LENGTH];
    strcpy(absolute_path, pathname);
    if (pathname[0] != '/')
    {
        get_absolute_path(absolute_path);
    }

    int32_t mp_id = get_mountpoint_id(absolute_path);
    if (mp_id < 0)
    {
        return -1;
    }

    if (mountpoint_list[mp_id].operations.remove_f == NULL)
    {
        return -1;
    }

    return mountpoint_list[mp_id].operations.remove_f(absolute_path);
}
