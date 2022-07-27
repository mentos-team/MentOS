/// @file stat.h
/// @brief Defines the structure used by the functiosn fstat(), lstat(), and stat().
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#if !defined(__SYS_STAT_H) && !defined(__KERNEL__)
#error "Never include <bits/stat.h> directly; use <sys/stat.h> instead."
#endif

#include "stddef.h"
#include "time.h"

/// @brief Data structure which contains information about a file.
typedef struct stat_t {
    /// ID of device containing file.
    dev_t st_dev;
    /// File serial number.
    ino_t st_ino;
    /// Mode of file.
    mode_t st_mode;
    /// File user id.
    uid_t st_uid;
    /// File group id.
    gid_t st_gid;
    /// File Size.
    off_t st_size;
    /// Time of last access.
    time_t st_atime;
    /// Time of last data modification.
    time_t st_mtime;
    /// Time of last status change.
    time_t st_ctime;
} stat_t;
