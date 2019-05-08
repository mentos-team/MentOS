///                MentOS, The Mentoring Operating system project
/// @file dirent.h
/// @brief Functions used to manage directories.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// The maximum length for a name.
#define NAME_MAX 30

/// @brief Contains the entries of a directory.
typedef struct dirent_t
{
    /// The inode of the entry.
    ino_t d_ino;

    /// The type of the entry.
    int d_type;

    /// The nam of the entry.
    char d_name[NAME_MAX + 1];
} dirent_t;

/// @brief Contains information concerning a directory.
typedef struct DIR
{
    /// Filesystem directory handle.
    int handle;

    /// The currently opened entry.
    ino_t cur_entry;

    /// Path to the directory.
    char path[NAME_MAX + 1];

    /// Next directory item.
    dirent_t entry;
} DIR;

/// @brief      Opens a directory and returns a handler to it.
/// @param path The path of the directory.
/// @return     Pointer to the directory. Otherwise, NULL.
DIR *opendir(const char *path);

/// @brief Given a pointer to a directory, it closes it.
int closedir(DIR *dirp);

/// @brief At each call of this function, it returns a pointer to the next
/// element inside the directory.
/// @return A pointer to the next element. If there are no more elments, it
/// returns NULL.
dirent_t *readdir(DIR *dirp);
