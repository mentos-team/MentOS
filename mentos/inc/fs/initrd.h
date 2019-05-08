///                MentOS, The Mentoring Operating system project
/// @file initrd.h
/// @brief Headers of functions for initrd filesystem.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stat.h"
#include "dirent.h"
#include "unistd.h"
#include "stdint.h"
#include "kernel.h"

/// The maximum number of files.
#define MAX_FILES 32

/// The maximum number of file descriptors.
#define MAX_INITRD_DESCRIPTORS MAX_OPEN_FD

/// @brief Contains the number of files inside the initrd filesystem.
typedef struct initrd_t {
	/// Number of files.
	uint32_t nfiles;
} initrd_t;

/// @brief Information concerning a file.
typedef struct initrd_file_t {
	/// Number used as delimiter, it must be set to 0xBF.
	int magic;
	/// The name of the file.
	char fileName[MAX_FILENAME_LENGTH];
	/// The type of the file.
	short int file_type;
	/// The uid of the owner.
	int uid;
	/// Offset of the starting address.
	unsigned int offset;
	/// Dimension of the file.
	unsigned int length;
} initrd_file_t;

/// @brief Descriptor linked to open files.
typedef struct initrd_fd {
	/// Id of the open file inside the file system. More precisely, its index
	/// inside the vector of files.
	int file_descriptor;
	/// The current position inside the file. Used for writing/reading
	/// opeations.
	int cur_pos;
} initrd_fd;

// TODO: doxygen comment.
/// @brief
extern initrd_t *fs_specs;
/// @brief File system headers.
extern initrd_file_t *fs_headers;
// TODO: doxygen comment.
/// @brief
extern unsigned int fs_end;

/// Initializes the initrd file system.
uint32_t initfs_init();

/// @brief      Opens a directory at the given path.
/// @param path The path where the directory resides.
/// @return     Structure used to access the directory.
DIR *initfs_opendir(const char *path);

/// @brief      Closes the directory stream associated with dirp.
/// @param dirp The directory handler.
/// @return 0   on success. On error, -1 is returned, and errno is set.
int initfs_closedir(DIR *dirp);

/// @brief      Moves the position of the currently readed entry inside the
///             directory.
/// @param dirp The directory handler.
/// @return     A pointer to the next entry inside the directory.
dirent_t *initrd_readdir(DIR *dirp);

/// @brief      Creates a new directory.
/// @param path The path to the new directory.
/// @param mode The file mode.
/// @return 0   if success.
int initrd_mkdir(const char *path, mode_t mode);

/// @brief      Removes a directory.
/// @param path The path to the directory.
/// @return 0   if success.
int initrd_rmdir(const char *path);

/// @brief       Open the file at the given path and returns its file descriptor.
/// @param path  The path to the file.
/// @param flags The flags used to determine the behavior of the function.
/// @return      The file descriptor of the opened file, otherwise returns -1.
int initfs_open(const char *path, int flags, ...);

/// @brief      Deletes the file at the given path.
/// @param path The path to the file.
/// @return     On success, zero is returned. On error, -1 is returned.
int initfs_remove(const char *path);

/// @brief        Reads from the file identified by the file descriptor.
/// @param fildes The file descriptor of the file.
/// @param buf    Buffer where the read content must be placed.
/// @param nbyte  The number of bytes to read.
/// @return T     The number of red bytes.
ssize_t initfs_read(int fildes, char *buf, size_t nbyte);

/// @brief      Retrieves information concerning the file at the given position.
/// @param path The path where the file resides.
/// @param stat The structure where the information are stored.
/// @return     0 if success.
int initrd_stat(const char *path, stat_t *stat);

/// @brief        Writes the given content inside the file.
/// @param fildes The file descriptor of the file.
/// @param buf    The content to write.
/// @param nbyte  The number of bytes to write.
/// @return T     The number of written bytes.
ssize_t initrd_write(int fildes, const void *buf, size_t nbyte);

/// @brief        Closes the given file.
/// @param fildes The file descriptor of the file.
int initrd_close(int fildes);

// TODO: doxygen comment.
size_t initrd_nfiles();

// TODO: doxygen comment.
void dump_initrd_fs();
