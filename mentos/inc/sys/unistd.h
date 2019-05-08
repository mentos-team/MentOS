///                MentOS, The Mentoring Operating system project
/// @file unistd.h
/// @brief Functions used to manage files.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "types.h"
#include "stddef.h"

//======== Standard file descriptors ===========================================
/// Standard input.
#define STDIN_FILENO -3

/// Standard output.
#define STDOUT_FILENO -2

/// Standard error output.
#define STDERR_FILENO -1
//==============================================================================

/// Maximum number of opened file.
#define MAX_OPEN_FD 4

/// @brief        Read data from a file descriptor.
/// @param fd     The file descriptor.
/// @param buf    The buffer.
/// @param nbytes The number of bytes to read.
/// @return       The number of read characters.
ssize_t read(int fd, void *buf, size_t nbytes);

/// @brief        Write data into a file descriptor.
/// @param fd     The file descriptor.
/// @param buf    The buffer collecting data to written.
/// @param nbytes The number of bytes to write.
/// @return       The number of written bytes.
ssize_t write(int fd, void *buf, size_t nbytes);

/// @brief          Opens the file specified by pathname.
/// @param pathname A pathname for a file.
/// @param flags    Used to set the file status flags and file access modes
///                 of the open file description.
/// @param mode     Specifies the file mode bits be applied when a new file
///                 is created.
/// @return         Returns a file descriptor, a small, nonnegative integer for
///                 use in subsequent system calls.
int open(const char *pathname, int flags, mode_t mode);

/// @brief        Close a file descriptor.
/// @param fildes The file descriptor.
/// @return       The result of the operation.
int close(int fildes);

/// @brief      Removes the given directory.
/// @param path The path to the directory to remove.
/// @return
int rmdir(const char *path);

/// @brief Wrapper for exit system call.
extern void exit(int status);

/// @brief        Return the process identifier of the calling process.
/// @return pid_t process identifier.
extern pid_t getpid();

/// @brief Clone the calling process, but without copying the whole address space.
///        The calling process is suspended until the new process exits or is
///        replaced by a call to `execve'.  Return -1 for errors, 0 to the new
///        process, and the process ID of the new process to the old process.
/// @return
extern pid_t vfork();

/// @brief      Replaces the current process image with a new process image.
/// @param path The path to the binary file to execute.
/// @param argv The list of arguments.
/// @param envp
/// @return
extern int execve(const char *path, char *const argv[], char *const envp[]);

/// @brief      Adds inc to the nice value for the calling thread.
/// @param inc  The value to add to the nice.
/// @return     On success, the new nice value is returned. On error, -1 is
///             returned, and errno is set appropriately.
int nice(int inc);

/// @brief      Reboot system call.
/// @param cmd
/// @param arg
/// @return
int reboot(int magic1, int magic2, unsigned int cmd, void *arg);

void getcwd(char *path, size_t size);

void chdir(char const *path);
