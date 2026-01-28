/// @file fcntl.h
/// @brief Header file for file control options and commands.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @name File Access Modes
/// @brief Defines file access modes for opening files.
/// @{
#define O_ACCMODE 0003      ///< Bits defining the open mode.
#define O_RDONLY  00000000U ///< Open for reading only.
#define O_WRONLY  00000001U ///< Open for writing only.
#define O_RDWR    00000002U ///< Open for reading and writing.
/// @}

/// @name File Creation Flags
/// @brief Flags for file creation, controlling how files are opened or created.
/// @{
#define O_CREAT     00000100U ///< Create file if it does not exist.
#define O_EXCL      00000200U ///< Error if file already exists.
#define O_NOCTTY    00000400U ///< Do not assign controlling terminal.
#define O_TRUNC     00001000U ///< Truncate file to zero length.
#define O_APPEND    00002000U ///< Set append mode.
#define O_NONBLOCK  00004000U ///< Enable non-blocking mode.
#define O_DIRECTORY 00200000U ///< Open only if it is a directory.
/// @}

/// @name fcntl Commands
/// @brief Commands for the fcntl() function, used for managing file descriptors.
/// @{
#define F_DUPFD  0 ///< Duplicate file descriptor.
#define F_GETFD  1 ///< Get file descriptor flags.
#define F_SETFD  2 ///< Set file descriptor flags.
#define F_GETFL  3 ///< Get file status flags.
#define F_SETFL  4 ///< Set file status flags.
#define F_GETOWN 5 ///< Get process receiving SIGIO signals.
#define F_SETOWN 6 ///< Set process receiving SIGIO signals.
#define F_GETLK  7 ///< Get record locking information.
#define F_SETLK  8 ///< Set record locking information.
#define F_SETLKW 9 ///< Set record locking info; wait if blocked.
/// @}

/// @name Lock Operation Flags
/// @brief Flags for the fcntl function lock operations.
/// @{
#define F_RDLCK 1 ///< Shared or read lock.
#define F_WRLCK 2 ///< Exclusive or write lock.
#define F_UNLCK 3 ///< Unlock.
/// @}

/// @brief Provides control operations on an open file descriptor.
/// @param fd The file descriptor on which to perform the operation.
/// @param request The `fcntl` command, defining the operation (e.g., `F_GETFL`, `F_SETFL`).
/// @param data Additional data required by certain `fcntl` commands (e.g., flags or pointer).
/// @return Returns 0 on success; on error, returns a negative error code.
long fcntl(int fd, unsigned int request, unsigned long data);
