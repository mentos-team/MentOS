///                MentOS, The Mentoring Operating system project
/// @file stat.c
/// @brief Stat functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "vfs.h"
#include "kheap.h"
#include "stdio.h"
#include "string.h"
#include "initrd.h"

int stat(const char *path, stat_t *buf)
{
    // Reset the structure.
    buf->st_dev = 0;
    buf->st_ino = 0;
    buf->st_mode = 0;
    buf->st_uid = 0;
    buf->st_gid = 0;
    buf->st_size = 0;
    buf->st_atime = 0;
    buf->st_mtime = 0;
    buf->st_ctime = 0;

    char absolute_path[MAX_PATH_LENGTH];
    strcpy(absolute_path, path);

    if (path[0] != '/')
    {
        get_absolute_path(absolute_path);
    }

    int32_t mp_id = get_mountpoint_id(absolute_path);
    if (mp_id < 0)
    {
        printf("stat: cannot execute stat of '%s': Not exists\n", path);
        return -1;
    }

    buf->st_dev = (uint32_t) mp_id;
    if (mountpoint_list[mp_id].stat_op.stat_f == NULL)
    {
        printf("stat: cannot execute stat of '%s': Not stat function\n",
               path);

        return -1;
    }
    mountpoint_list[mp_id].stat_op.stat_f(absolute_path, buf);

    return 0;
}

int mkdir(const char *path, mode_t mode)
{
    char absolute_path[MAX_PATH_LENGTH];
    strcpy(absolute_path, path);
    int result = -1;

    if (path[0] != '/')
    {
        get_absolute_path(absolute_path);
    }

    int32_t mp_id = get_mountpoint_id(absolute_path);
    if (mp_id < 0)
    {
        printf("mkdir: cannot create directory '%s':"
               "Cannot find mount-point\n", path);

        return result;
    }

    if (mountpoint_list[mp_id].dir_op.mkdir_f == NULL)
    {
        printf("mkdir: cannot create directory '%s': "
               "No mkdir function\n", path);

        return result;
    }
    result = mountpoint_list[mp_id].dir_op.mkdir_f(absolute_path, mode);

    return result;
}
