///                MentOS, The Mentoring Operating system project
/// @file namei.c
/// @brief Implementation of functions fcntl() and open().
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/errno.h"
#include "fs/vfs.h"
#include "process/scheduler.h"

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
