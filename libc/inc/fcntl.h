/// @file fcntl.h
/// @brief Headers of functions fcntl() and open().
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#define O_ACCMODE   0003      ///< Bits defining the open mode
#define O_RDONLY    00000000U ///< Open for reading only.
#define O_WRONLY    00000001U ///< Open for writing only.
#define O_RDWR      00000002U ///< Open for reading and writing.
#define O_CREAT     00000100U ///< Create if nonexistant.
#define O_EXCL      00000200U ///< Error if already exists.
#define O_TRUNC     00001000U ///< Truncate to zero length.
#define O_APPEND    00002000U ///< Set append mode.
#define O_NONBLOCK  00004000U ///< No delay.
#define O_DIRECTORY 00200000U ///< If file exists has no effect. Otherwise, the file is created.

/// @defgroup ModeBitsAccessPermission Mode Bits for Access Permission
/// @brief The file modes.
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

#define S_ISDIR(m)  (((m)&0170000) == 0040000) ///< directory.
#define S_ISCHR(m)  (((m)&0170000) == 0020000) ///< char special
#define S_ISBLK(m)  (((m)&0170000) == 0060000) ///< block special
#define S_ISREG(m)  (((m)&0170000) == 0100000) ///< regular file
#define S_ISFIFO(m) (((m)&0170000) == 0010000) ///< fifo
#define S_ISLNK(m)  (((m)&0170000) == 0120000) ///< symbolic link
#define S_ISSOCK(m) (((m)&0170000) == 0140000) ///< socket

/// @}
