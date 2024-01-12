/// @file   syscall.h
/// @brief  System Call handler definition.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "system/syscall_types.h"
#include "fs/vfs_types.h"
#include "kernel.h"
#include "sys/dirent.h"
#include "sys/types.h"

/// @brief Initialize the system calls.
void syscall_init(void);

/// @brief Handler for the system calls.
/// @param f The interrupt stack frame.
void syscall_handler(pt_regs *f);

/// @brief Returns the current interrupt stack frame.
/// @return Pointer to the stack frame.
pt_regs *get_current_interrupt_stack_frame(void);

/// The exit() function causes normal process termination.
/// @param exit_code The exit code.
void sys_exit(int exit_code);

/// @brief Read data from a file descriptor.
/// @param fd     The file descriptor.
/// @param buf    The buffer.
/// @param nbytes The number of bytes to read.
/// @return The number of read characters.
ssize_t sys_read(int fd, void *buf, size_t nbytes);

/// @brief Write data into a file descriptor.
/// @param fd     The file descriptor.
/// @param buf    The buffer collecting data to written.
/// @param nbytes The number of bytes to write.
/// @return The number of written bytes.
ssize_t sys_write(int fd, const void *buf, size_t nbytes);

/// @brief Repositions the file offset inside a file.
/// @param fd     The file descriptor of the file.
/// @param offset The offest to use for the operation.
/// @param whence The type of operation.
/// @return  Upon successful completion, returns the resulting offset
/// location as measured in bytes from the beginning of the file. On
/// error, the value (off_t) -1 is returned and errno is set to
/// indicate the error.
off_t sys_lseek(int fd, off_t offset, int whence);

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

/// @brief Delete a name and possibly the file it refers to.
/// @param path A pathname for a file.
/// @return On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
int sys_unlink(const char *path);

/// @brief Suspends execution of the calling thread until a child specified
///        by pid argument has changed state.
/// @param pid     The pid to wait.
/// @param status  If not NULL, store status information here.
/// @param options Determines the wait behaviour.
/// @return on success, returns the process ID of the terminated
///         child; on error, -1 is returned.
pid_t sys_waitpid(pid_t pid, int *status, int options);

/// @brief Replaces the current process image with a new process image.
/// @param f CPU registers whe calling this function.
/// @return 0 on success, -1 on error.
int sys_execve(pt_regs *f);

/// @brief Changes the working directory.
/// @param path The new working directory.
/// @return On success 0. On error -1, and errno indicates the error.
int sys_chdir(char const *path);

/// @brief Changes the working directory.
/// @param fd File descriptor of the new working directory.
/// @return On success 0. On error -1, and errno indicates the error.
int sys_fchdir(int fd);

/// @brief Returns the process ID (PID) of the calling process.
/// @return The process ID.
pid_t sys_getpid(void);

///@brief  Return session id of the given process.
///        If pid == 0 return the SID of the calling process
///        If pid != 0 return the SID corresponding to the process having identifier == pid
///@param pid process identifier from wich we want the SID
///@return On success return SID of the session
///        Otherwise return -1 with errno set on: EPERM or ESRCH
pid_t sys_getsid(pid_t pid);

///@brief creates a new session if the calling process is not a
///       process group leader.  The calling process is the leader of the
///       new session (i.e., its session ID is made the same as its process
///       ID).  The calling process also becomes the process group leader
///       of a new process group in the session (i.e., its process group ID
///       is made the same as its process ID).
///@return On success return SID of the session just created
///        Otherwise return -1 with errno : EPERM
pid_t sys_setsid(void);

///@brief returns the Process Group ID (PGID) of the process specified by pid.
/// If pid is zero, the process ID of the calling process is used.
/// @param pid process of which we want to know the PGID.
/// @return the PGID of the specified process.
pid_t sys_getpgid(pid_t pid);

/// @brief Sets the Process Group ID (PGID) of the process specified by pid.
/// If pid is zero, the process ID of the calling process is used.
/// @param pid process of which we want to set the PGID.
/// @param pgid the PGID we want to set.
/// @return returns zero. On error, -1 is returned, and errno is set appropriately.
int sys_setpgid(pid_t pid, pid_t pgid);

///@brief returns the real group ID of the calling process.
///@return GID of the current process
pid_t sys_getgid(void);

///@brief sets the group ID of the calling process.
///@param pid process identifier to
///@return On success, zero is returned.
///        Otherwise returns -1 with errno set to :EINVAL or EPERM.
int sys_setgid(pid_t pid);

///@brief returns the effective group ID of the calling process.
///@return GID of the current process
gid_t sys_getegid(void);

///@brief sets the real and effective group ID of the calling process.
///@param rgid real group id
///@param egid effective group id
///@return On success, zero is returned.
///        Otherwise returns -1 with errno set to EPERM.
int sys_setregid(gid_t rgid, gid_t egid);

/// @brief Returns the parent process ID (PPID) of the calling process.
/// @return The parent process ID.
pid_t sys_getppid(void);

/// @brief Returns the real User ID (UID) of the calling process.
/// @return The User ID.
uid_t sys_getuid(void);

/// @brief Tries to set the User ID (UID) of the calling process.
/// @param uid the new User ID.
///@return On success, zero is returned.
///        Otherwise returns -1 with errno set to :EINVAL or EPERM.
int sys_setuid(uid_t uid);

/// @brief Returns the effective User ID (UID) of the calling process.
/// @return The User ID.
uid_t sys_geteuid(void);

/// @brief Set the real and effective User ID (UID) of the calling process.
/// @param ruid the new real User ID.
/// @param euid the new effective User ID.
///@return On success, zero is returned.
///        Otherwise returns -1 with errno set to EPERM.
int sys_setreuid(uid_t ruid, uid_t euid);

/// @brief Adds the increment to the priority value of the task.
/// @param increment The modifier to apply to the nice value.
/// @return The new nice value.
int sys_nice(int increment);

/// @brief Reboots the system, or enables/disables the reboot keystroke.
/// @param magic1 fails (with the error EINVAL) unless equals LINUX_REBOOT_MAGIC1.
/// @param magic2 fails (with the error EINVAL) unless equals LINUX_REBOOT_MAGIC2.
/// @param cmd The command to send to the reboot.
/// @param arg Argument passed with some specific commands.
/// @return For the values of cmd that stop or restart the system, a
///         successful call to reboot() does not return. For the other cmd
///         values, zero is returned on success. In all cases, -1 is
///         returned on failure, and errno is set appropriately.
int sys_reboot(int magic1, int magic2, unsigned int cmd, void *arg);

/// @brief Get current working directory.
/// @param buf  The array where the CWD will be copied.
/// @param size The size of the array.
/// @return On success, returns the same pointer to buf.
///         On failure, returnr NULL, and errno is set to indicate the error.
char *sys_getcwd(char *buf, size_t size);

/// @brief Clone the calling process, but without copying the whole address space.
///        The calling process is suspended until the new process exits or is
///        replaced by a call to `execve'.
/// @param f CPU registers whe calling this function.
/// @return Return -1 for errors, 0 to the new process, and the process ID of
///         the new process to the old process.
pid_t sys_fork(pt_regs *f);

/// @brief Stat the file at the given path.
/// @param path Path to the file for which we are retrieving the statistics.
/// @param buf  Buffer where we are storing the statistics.
/// @return 0 on success, a negative number if fails and errno is set.
int sys_stat(const char *path, stat_t *buf);

/// @brief Retrieves information about the file at the given location.
/// @param fd  The file descriptor of the file that is being inquired.
/// @param buf A structure where data about the file will be stored.
/// @return Returns a negative value on failure.
int sys_fstat(int fd, stat_t *buf);

/// @brief Creates a new directory at the given path.
/// @param path The path of the new directory.
/// @param mode The permission of the new directory.
/// @return Returns a negative value on failure.
int sys_mkdir(const char *path, mode_t mode);

/// @brief Removes the given directory.
/// @param path The path to the directory to remove.
/// @return Returns a negative value on failure.
int sys_rmdir(const char *path);

/// @brief Creates a new file or rewrite an existing one.
/// @param path path to the file.
/// @param mode mode for file creation.
/// @return file descriptor number, -1 otherwise and errno is set to indicate the error.
/// @details
/// It is equivalent to: open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)
int sys_creat(const char *path, mode_t mode);

/// @brief Read the symbolic link, if present.
/// @param file the file for which we want to read the symbolic link information.
/// @param buffer the buffer where we will store the symbolic link path.
/// @param bufsize the size of the buffer.
/// @return The number of read characters on success, -1 otherwise and errno is set to indicate the error.
int sys_readlink(const char *path, char *buffer, size_t bufsize);

/// @brief Creates a symbolic link.
/// @param linkname the name of the link.
/// @param path the entity it is linking to.
/// @return 0 on success, a negative number if fails and errno is set.
int sys_symlink(const char *linkname, const char *path);

/// Provide access to the directory entries.
/// @param fd    The file descriptor of the directory for which we accessing
///              the entries.
/// @param dirp  The buffer where de data should be placed.
/// @param count The size of the buffer.
/// @return On success, the number of bytes read is returned.  On end of
///         directory, 0 is returned.  On error, -1 is returned, and errno is set
///         appropriately.
ssize_t sys_getdents(int fd, dirent_t *dirp, unsigned int count);

/// @brief Returns the current time.
/// @param time Where the time should be stored.
/// @return The current time.
time_t sys_time(time_t *time);
