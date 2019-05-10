///                MentOS, The Mentoring Operating system project
/// @file   syscall.h
/// @brief  System Call handler definition.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "syscall_types.h"
#include "kernel.h"
#include "dirent.h"
#include "types.h"
#include "stat.h"

/// @brief Initialize the system calls.
void syscall_init();

/// @brief Handler for the system calls.
void syscall_handler(pt_regs *r);

/// The exit() function causes normal process termination.
void sys_exit(int exit_code);

/// @brief        Read data from a file descriptor.
/// @param fd     The file descriptor.
/// @param buf    The buffer.
/// @param nbytes The number of bytes to read.
/// @return       The number of read characters.
ssize_t sys_read(int fd, void *buf, size_t nbytes);

/// @brief        Write data into a file descriptor.
/// @param fd     The file descriptor.
/// @param buf    The buffer collecting data to written.
/// @param nbytes The number of bytes to write.
/// @return       The number of written bytes.
ssize_t sys_write(int fd, void *buf, size_t nbytes);

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

/// @brief Suspends execution of the calling thread until a child specified
/// by pid argument has changed state.
pid_t sys_waitpid(pid_t pid, int *status, int options);

/// @brief Replaces the current process image with a new process image.
int sys_execve(pt_regs *r);

void sys_chdir(char const *path);

/// Returns the process ID (PID) of the calling process.
pid_t sys_getpid();

/// @brief Adds the increment to the priority value of the task.
int sys_nice(int increment);

/// Returns the parent process ID (PPID) of the calling process.
pid_t sys_getppid();

int sys_reboot(int magic1, int magic2, unsigned int cmd, void *arg);

void sys_getcwd(char *path, size_t size);

/// @brief Create a child process.
pid_t sys_vfork(pt_regs *r);

int sys_stat(const char *path, stat_t *buf);

int sys_mkdir(const char *path, mode_t mode);

dirent_t *sys_readdir(DIR *dirp);