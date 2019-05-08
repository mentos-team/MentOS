///                MentOS, The Mentoring Operating system project
/// @file stat.h
/// @brief Stat functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "clock.h"
#include "stddef.h"

/// @brief Data structure which contains information about a file.
typedef struct stat_t
{
    /// ID of device containing file.
    dev_t st_dev;
    /// Fle serial number.
    ino_t st_ino;
    /// Mode of file.
    mode_t st_mode;
    /// User id del file.
    uid_t st_uid;
    /// Group id del file.
    gid_t st_gid;
    /// Dimensione del file.
    off_t st_size;
    /// Time of last access.
    time_t st_atime;
    /// Time of last data modification.
    time_t st_mtime;
    /// Time of last status change.
    time_t st_ctime;
} stat_t;

/// @brief      Retrieves information about the file at the given location.
/// @param path The file descriptor of the file that is being inquired.
/// @param buf  A structure where data about the file will be stored.
/// @return     Returns a negative value on failure.
int stat(const char *path, stat_t *buf);

// TODO: doxygen comment.
int mkdir(const char *path, mode_t mode);
