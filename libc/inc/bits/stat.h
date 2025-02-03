/// @file stat.h
/// @brief Defines the structure used by the functiosn fstat(), lstat(), and stat().
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#if !defined(__SYS_STAT_H) && !defined(__KERNEL__)
#error "Never include <bits/stat.h> directly; use <sys/stat.h> instead."
#endif

#include "stddef.h"
#include "sys/types.h"
#include "time.h"

/// @brief Data structure which contains information about a file.
typedef struct stat {
    /// ID of device containing file.
    dev_t st_dev;
    /// File serial number.
    ino_t st_ino;
    /// Mode of file (file type and permissions).
    mode_t st_mode;
    /// Number of hard links to the file.
    nlink_t st_nlink;
    /// File user ID.
    uid_t st_uid;
    /// File group ID.
    gid_t st_gid;
    /// Device ID (if special file).
    dev_t st_rdev;
    /// File size in bytes.
    off_t st_size;
    /// Preferred block size for filesystem I/O.
    blksize_t st_blksize;
    /// Number of blocks allocated for the file.
    blkcnt_t st_blocks;
    /// Time of last access.
    time_t st_atime;
    /// Time of last data modification.
    time_t st_mtime;
    /// Time of last status change.
    time_t st_ctime;
} stat_t;
