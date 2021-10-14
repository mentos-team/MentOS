///                MentOS, The Mentoring Operating system project
/// @file stat.h
/// @brief Stat functions.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once
#define __SYS_STAT_H

#include "bits/stat.h"
#include "stddef.h"
#include "time.h"

/// @brief Retrieves information about the file at the given location.
/// @param path The path to the file that is being inquired.
/// @param buf  A structure where data about the file will be stored.
/// @return Returns a negative value on failure.
int stat(const char *path, stat_t *buf);

/// @brief Retrieves information about the file at the given location.
/// @param fd  The file descriptor of the file that is being inquired.
/// @param buf A structure where data about the file will be stored.
/// @return Returns a negative value on failure.
int fstat(int fd, stat_t *buf);

/// @brief Creates a new directory at the given path.
/// @param path The path of the new directory.
/// @param mode The permission of the new directory.
/// @return Returns a negative value on failure.
int mkdir(const char *path, mode_t mode);

/// @brief Removes the given directory.
/// @param path The path to the directory to remove.
/// @return Returns a negative value on failure.
int rmdir(const char *path);