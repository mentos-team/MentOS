///                MentOS, The Mentoring Operating system project
/// @file fcntl.h
/// @brief Headers of functions fcntl() and open().
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// Open for reading only.
#define O_RDONLY 0x00

/// Open for writing only.
#define O_WRONLY 0x01

/// Open for reading and writing.
#define O_RDWR 0x02

/// If the file exists, this flag has no effect. Otherwise, the file is created.
#define O_CREAT 0x40

/// The file offset will be set to the end of the file prior to each write.
#define O_APPEND 0x400

/// @brief          Given a pathname for a file, open() returns a file
///                 descriptor, a small, nonnegative integer for use in
///                 subsequent system calls.
/// @param pathname A pathname for a file.
/// @param flags    Used to set the file status flags and file access modes
///                 of the open file description.
/// @param mode     Specifies the file mode bits be applied when a new file
///                 is created.
/// @return         Returns a file descriptor, a small, nonnegative integer
///                 for use in subsequent system calls.
int sys_open(const char *pathname, int flags, mode_t mode);

/// @brief          Deletes the file whose name is specified in filename.
/// @param pathname A path to a file.
/// @return         On success, zero is returned. On error, -1 is returned,
///                 and errno is set appropriately.
int remove(const char *pathname);
