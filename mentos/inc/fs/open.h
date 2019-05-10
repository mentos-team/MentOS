///                MentOS, The Mentoring Operating system project
/// @file open.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stddef.h"
#include "vfs.h"

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

/// @brief
/// @param fd
/// @return
int sys_close(int fd);