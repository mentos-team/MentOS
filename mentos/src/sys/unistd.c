///                MentOS, The Mentoring Operating system project
/// @file unistd.c
/// @brief Functions used to manage files.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "vfs.h"
#include "stdio.h"
#include "fcntl.h"
#include "kheap.h"
#include "errno.h"
#include "initrd.h"
#include "string.h"
#include "unistd.h"

int close(int fildes)
{
    if (fildes < 0)
    {
        return -1;
    }
    if (fd_list[fildes].fs_spec_id >= -1)
    {
        int mp_id = fd_list[fildes].mountpoint_id;
        if (mountpoint_list[mp_id].operations.close_f != NULL)
        {
            int fs_fd = fd_list[fildes].fs_spec_id;
            mountpoint_list[mp_id].operations.close_f(fs_fd);
        }
        fd_list[fildes].fs_spec_id = -1;
        fd_list[fildes].mountpoint_id = -1;
    }
    else
    {
        return -1;
    }

    return 0;
}

int rmdir(const char *path)
{
    char absolute_path[MAX_PATH_LENGTH];
    strcpy(absolute_path, path);

    if (path[0] != '/')
    {
        get_absolute_path(absolute_path);
    }

    int32_t mp_id = get_mountpoint_id(absolute_path);
    if (mp_id < 0)
    {
        printf("rmdir: failed to remove '%s':"
               "Cannot find mount-point\n", path);

        return -1;
    }

    if (mountpoint_list[mp_id].dir_op.rmdir_f == NULL)
    {
        return -1;
    }

    return mountpoint_list[mp_id].dir_op.rmdir_f(absolute_path);
}
