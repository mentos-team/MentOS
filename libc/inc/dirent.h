/// @file dirent.h
/// @brief Functions used to manage directories.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "limits.h"
#include "stddef.h"

/// File types for `d_type'.
enum {
    DT_UNKNOWN = 0,
    DT_FIFO    = 1,
    DT_CHR     = 2,
    DT_DIR     = 4,
    DT_BLK     = 6,
    DT_REG     = 8,
    DT_LNK     = 10,
    DT_SOCK    = 12,
    DT_WHT     = 14
};

/// @brief Characters describing the direactory entry.
static const char dt_char_array[] = {
    '?', // DT_UNKNOWN = 0,
    'p', // DT_FIFO = 1,
    'c', // DT_CHR  = 2,
    '*',
    'd', // DT_DIR  = 4,
    '*',
    'b', // DT_BLK  = 6,
    '*',
    '-', // DT_REG  = 8,
    '*',
    'l', // DT_LNK  = 10,
    '*',
    's', // DT_SOCK = 12,
    '*',
    '?', // DT_WHT  = 14
};

/// Directory entry.
typedef struct dirent {
    ino_t d_ino;             ///< Inode number.
    off_t d_off;             ///< Offset to next linux_dirent.
    unsigned short d_reclen; ///< Length of this linux_dirent.
    unsigned short d_type;   ///< type of the directory entry.
    char d_name[NAME_MAX];   ///< Filename (null-terminated)
} dirent_t;

/// Provide access to the directory entries.
/// @param fd The fd pointing to the opened directory.
/// @param dirp The buffer where de data should be placed.
/// @param count The size of the buffer.
/// @return On success, the number of bytes read is returned.  On end of
///         directory, 0 is returned.  On error, -1 is returned, and errno is set
///         appropriately.
ssize_t getdents(int fd, dirent_t *dirp, unsigned int count);
