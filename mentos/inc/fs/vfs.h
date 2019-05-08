///                MentOS, The Mentoring Operating system project
/// @file vfs.h
/// @brief Headers for Virtual File System (VFS).
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "unistd.h"
#include "vfs_types.h"

/// The maximum number of mount points.
#define MAX_MOUNTPOINT 10

/// The currently opened file descriptor.
extern int current_fd;

/// The list of file descriptors.
extern file_descriptor_t fd_list[MAX_OPEN_FD];

/// The list of mount points.
extern mountpoint_t mountpoint_list[MAX_MOUNTPOINT];

/// @brief Initialize the Virtual File System (VFS).
void vfs_init();

/// @brief      Retrieves the id of the mount point where the path resides.
/// @param path The path to the mountpoint.
/// @return     The id of the mountpoint.
int32_t get_mountpoint_id(const char *path);

// TODO: doxigen comment.
mountpoint_t * get_mountpoint(const char *path);

// TODO: doxigen comment.
mountpoint_t * get_mountpoint_from_id(int32_t mp_id);

/// @brief       A path is extracted from the relative path, excluding the
///              mountpoint.
/// @param mp_id Id of the mountpoint point of the file.
/// @param path  Path to the file to be opened.
/// @return      Path without the mountpoint part.
int get_relative_path(uint32_t mp_id, char *path);

/// @brief      Given a path, it extracts its absolute path (starting from
///             the current one).
/// @param path Path to the file to be opened.
/// @return     Error code.
int get_absolute_path(char *path);

/// @brief Dumps the list of file descriptors.
void vfs_dump();
