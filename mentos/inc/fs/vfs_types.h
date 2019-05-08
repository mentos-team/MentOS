///                MentOS, The Mentoring Operating system project
/// @file   vfs_types.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stat.h"
#include "kernel.h"
#include "dirent.h"

/// Identifies a file.
#define FS_FILE 0x01

/// Identifies a directory.
#define FS_DIRECTORY 0x02

/// Identifies a character devies.
#define FS_CHARDEVICE 0x04

/// Identifies a block devies.
#define FS_BLOCKDEVICE 0x08

/// Identifies a pipe.
#define FS_PIPE 0x10

/// Identifies a symbolic link.
#define FS_SYMLINK 0x20

/// Identifies a mount-point.
#define FS_MOUNTPOINT 0x40

/// Function used to open a directory.
typedef DIR *(*opendir_callback)(const char *);

/// Function used to close a directory.
typedef int (*closedir_callback)(DIR *);

/// Function used to create a directory.
typedef int (*mkdir_callback)(const char *, mode_t);

/// Function used to remove a directory.
typedef int (*rmdir_callback)(const char *);

/// Function used to read the next entry of a directory.
typedef dirent_t *(*readdir_callback)(DIR *);

/// Function used to open a file.
typedef int (*open_callback)(const char *, int, ...);

/// Function used to remove a file.
typedef int (*remove_callback)(const char *);

/// Function used to close a file.
typedef int (*close_callback)(int);

/// Function used to read from a file.
typedef ssize_t (*read_callback)(int, char *, size_t);

/// Function used to write inside a file.
typedef ssize_t (*write_callback)(int, const void *, size_t);

/// Function used to stat fs entries.
typedef int (*stat_callback)(const char *, stat_t *);

/// @brief Set of functions used to perform operations on directories.
typedef struct directory_operations_t {
	/// Identifies a mount-point.
	opendir_callback opendir_f;
	/// Closes a directory.
	closedir_callback closedir_f;
	/// Creates a directory.
	mkdir_callback mkdir_f;
	/// Removes a directory.
	rmdir_callback rmdir_f;
	/// Read next entry inside the directory.
	readdir_callback readdir_f;
} directory_operations_t;

/// @brief Set of functions used to perform operations on files.
typedef struct super_node_operations_t {
	/// Open a file.
	open_callback open_f;
	/// Remove a file.
	remove_callback remove_f;
	/// Close a file.
	close_callback close_f;
	/// Read from a file.
	read_callback read_f;
	/// Write inside a file.
	write_callback write_f;
} super_node_operations_t;

/// @brief Stat operations.
typedef struct stat_operations_t {
	/// Stat function.
	stat_callback stat_f;
} stat_operations_t;

/// @brief Data structure that contains information about the mounted filesystems.
typedef struct mountpoint_t {
	/// The id of the mountpoint.
	int32_t mp_id;
	/// Name of the mountpoint.
	char mountpoint[MAX_FILENAME_LENGTH];
	/// Maschera dei permessi.
	unsigned int pmask;
	/// User ID.
	unsigned int uid;
	/// Group ID.
	unsigned int gid;
	/// Starting address of the FileSystem.
	unsigned int start_address;
	/// Ending address of the FileSystem.
	unsigned int end_address;
	/// Device ID.
	int dev_id;
	/// Operations on files.
	super_node_operations_t operations;
	/// Operations on directories.
	directory_operations_t dir_op;
	/// Stat operations.
	stat_operations_t stat_op;
} mountpoint_t;

/// @brief Data structure containing information about an open file.
typedef struct file_descriptor_t {
	/// The descriptor of the internal file of the FileSystem.
	int fs_spec_id;
	/// The id of the mountpoint where the.
	int mountpoint_id;
	/// Offset for file reading, for the next read.
	int offset;
	/// Flags for file opening modes.
	int flags_mask;
} file_descriptor_t;
