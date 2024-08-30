/// @file stat.h
/// @brief Stat functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once
#define __SYS_STAT_H

#include "bits/stat.h"
#include "stddef.h"
#include "time.h"

/// @defgroup FileTypes File Types Macros
/// @brief These constants allow to identify file types.
/// @{
#define S_IFMT   0xF000 ///< Format mask
#define S_IFSOCK 0xC000 ///< Socket
#define S_IFLNK  0xA000 ///< Symbolic link
#define S_IFREG  0x8000 ///< Regular file
#define S_IFBLK  0x6000 ///< Block device
#define S_IFDIR  0x4000 ///< Directory
#define S_IFCHR  0x2000 ///< Character device
#define S_IFIFO  0x1000 ///< Fifo
/// @}

/// @defgroup FileTypeTest File Type Test Macros
/// @brief These macros allows to easily identify file types.
/// @{
#define S_ISTYPE(mode, mask) (((mode) & S_IFMT) == (mask))
#define S_ISSOCK(mode)       (S_ISTYPE(mode, S_IFSOCK)) ///< Check if a socket.
#define S_ISLNK(mode)        (S_ISTYPE(mode, S_IFLNK))  ///< Check if a symbolic link.
#define S_ISREG(mode)        (S_ISTYPE(mode, S_IFREG))  ///< Check if a regular file.
#define S_ISBLK(mode)        (S_ISTYPE(mode, S_IFBLK))  ///< Check if a block special.
#define S_ISDIR(mode)        (S_ISTYPE(mode, S_IFDIR))  ///< Check if a directory.
#define S_ISCHR(mode)        (S_ISTYPE(mode, S_IFCHR))  ///< Check if a char special.
#define S_ISFIFO(mode)       (S_ISTYPE(mode, S_IFIFO))  ///< Check if a fifo.
/// @}

/// @defgroup ModeBitsAccessPermission Mode Bits for Access Permission
/// @brief These constants allow to control access permission for files.
/// @{
#define S_ISUID 0x0800 ///< Set user id on execution
#define S_ISGID 0x0400 ///< Set group id on execution
#define S_ISVTX 0x0200 ///< Save swapped text even after use (Sticky Bit)
#define S_IRWXU 0x01C0 ///< rwx------ : User can read/write/execute
#define S_IRUSR 0x0100 ///< r-------- : User can read
#define S_IWUSR 0x0080 ///< -w------- : User can write
#define S_IXUSR 0x0040 ///< --x------ : User can execute
#define S_IRWXG 0x0038 ///< ---rwx--- : Group can read/write/execute
#define S_IRGRP 0x0020 ///< ---r----- : Group can read
#define S_IWGRP 0x0010 ///< ----w---- : Group can write
#define S_IXGRP 0x0008 ///< -----x--- : Group can execute
#define S_IRWXO 0x0007 ///< ------rwx : Others can read/write/execute
#define S_IROTH 0x0004 ///< ------r-- : Others can read
#define S_IWOTH 0x0002 ///< -------w- : Others can write
#define S_IXOTH 0x0001 ///< --------x : Others can execute
/// @}

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

/// @brief Creates a new file or rewrite an existing one.
/// @param path path to the file.
/// @param mode mode for file creation.
/// @return file descriptor number, -1 otherwise and errno is set to indicate the error.
/// @details
/// It is equivalent to: open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)
int creat(const char *path, mode_t mode);
