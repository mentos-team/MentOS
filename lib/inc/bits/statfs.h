/// @file statfs.h
/// @brief Defines the structure used by statfs() and fstatfs().
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#if !defined(__SYS_STATFS_H) && !defined(__KERNEL__)
#error "Never include <bits/statfs.h> directly; use <sys/statfs.h> instead."
#endif

#include "stdint.h"

typedef struct statfs {
    /// Type of filesystem.
    uint32_t f_type;
    /// Optimal transfer block size.
    uint32_t f_bsize;
    /// Total data blocks in filesystem.
    uint32_t f_blocks;
    /// Free blocks in filesystem.
    uint32_t f_bfree;
    /// Free blocks available to unprivileged users.
    uint32_t f_bavail;
    /// Total file nodes in filesystem.
    uint32_t f_files;
    /// Free file nodes in filesystem.
    uint32_t f_ffree;
    /// Filesystem ID.
    uint32_t f_fsid;
    /// Maximum length of filenames.
    uint32_t f_namelen;
    /// Fragment size.
    uint32_t f_frsize;
    /// Mount flags.
    uint32_t f_flags;
    /// Spare values for future extensions.
    uint32_t f_spare[4];
} statfs_t;
