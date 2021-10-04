///                MentOS, The Mentoring Operating system project
/// @file fcntl.h
/// @brief Headers of functions fcntl() and open().
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

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

#define S_IRWXU 0000700U ///< RWX mask for user.
#define S_IRUSR 0000400U ///< R for user.
#define S_IWUSR 0000200U ///< W for user.
#define S_IXUSR 0000100U ///< X for user.

#define S_IRWXG 0000070U ///< RWX mask for group.
#define S_IRGRP 0000040U ///< R for group.
#define S_IWGRP 0000020U ///< W for group.
#define S_IXGRP 0000010U ///< X for group.

#define S_IRWXO 0000007U ///< RWX mask for group.
#define S_IROTH 0000004U ///< R for group.
#define S_IWOTH 0000002U ///< W for group.
#define S_IXOTH 0000001U ///< X for group.

#define S_ISDIR(m)  (((m)&0170000) == 0040000) ///< directory.
#define S_ISCHR(m)  (((m)&0170000) == 0020000) ///< char special
#define S_ISBLK(m)  (((m)&0170000) == 0060000) ///< block special
#define S_ISREG(m)  (((m)&0170000) == 0100000) ///< regular file
#define S_ISFIFO(m) (((m)&0170000) == 0010000) ///< fifo
#define S_ISLNK(m)  (((m)&0170000) == 0120000) ///< symbolic link
#define S_ISSOCK(m) (((m)&0170000) == 0140000) ///< socket

/// @}
