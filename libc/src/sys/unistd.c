/// @file unistd.c
/// @brief Functions used to manage files.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "string.h"
#include "limits.h"
#include "stdio.h"

#if 0
#include "vfs.h"

int rmdir(const char *path)
{
    char absolute_path[PATH_MAX];

    get_absolute_normalized_path(path, absolute_path);

    int32_t mp_id = get_mountpoint_id(absolute_path);
    if (mp_id < 0) {
        printf("rmdir: failed to remove '%s':"
               "Cannot find mount-point\n",
               path);

        return -1;
    }

    if (mountpoint_list[mp_id].dir_op.rmdir_f == NULL) {
        return -1;
    }

    return mountpoint_list[mp_id].dir_op.rmdir_f(absolute_path);
}

#endif
