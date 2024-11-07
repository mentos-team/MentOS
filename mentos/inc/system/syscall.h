/// @file   syscall.h
/// @brief  System Call handler definition.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "system/syscall_types.h"
#include "fs/vfs_types.h"
#include "sys/utsname.h"
#include "sys/types.h"
#include "sys/msg.h"
#include "sys/sem.h"
#include "sys/shm.h"
#include "kernel.h"
#include "dirent.h"

/// @brief Initialize the system calls.
void syscall_init(void);

pt_regs *get_current_interrupt_stack_frame(void);

/// @brief Handler for the system calls.
/// @param f The interrupt stack frame.
void syscall_handler(pt_regs *f);

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
gid_t sys_getgid(void);

///@brief sets the group ID of the calling process.
///@param gid group id
///@return On success, zero is returned.
///        Otherwise returns -1 with errno set to :EINVAL or EPERM.
int sys_setgid(gid_t gid);

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
/// @param path the file for which we want to read the symbolic link information.
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

/// @brief Get a System V semaphore set identifier.
/// @param key can be used either to obtain the identifier of a previously
/// created semaphore set, or to create a new set.
/// @param nsems number of semaphores.
/// @param semflg controls the behaviour of the function.
/// @return the semaphore set identifier, -1 on failure, and errno is set to
/// indicate the error.
long sys_semget(key_t key, int nsems, int semflg);

/// @brief Performs operations on selected semaphores in the set.
/// @param semid the semaphore set identifier.
/// @param sops specifies operations to be performed on single semaphores.
/// @param nsops number of operations.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long sys_semop(int semid, struct sembuf *sops, unsigned nsops);

/// @brief Performs control operations on a semaphore set.
/// @param semid the semaphore set identifier.
/// @param semnum the n-th semaphore of the set on which we perform the operations.
/// @param cmd the command to perform.
/// @param arg
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long sys_semctl(int semid, int semnum, int cmd, union semun *arg);

/// @brief Get a System V shared memory identifier.
/// @param key can be used either to obtain the identifier of a previously
/// created shared memory, or to create a new one.
/// @param size of the shared memory, rounded up to a multiple of PAGE_SIZE.
/// @param shmflg controls the behaviour of the function.
/// @return the shared memory identifier, -1 on failure, and errno is set to
/// indicate the error.
long sys_shmget(key_t key, size_t size, int shmflg);

/// @brief Attaches the shared memory segment identified by shmid to the address
/// space of the calling process.
/// @param shmid the shared memory identifier.
/// @param shmaddr the attaching address.
/// @param shmflg controls the behaviour of the function.
/// @return returns the address of the attached shared memory segment; on error
/// (void *) -1 is returned, and errno is set to indicate the error.
void *sys_shmat(int shmid, const void *shmaddr, int shmflg);

/// @brief Detaches the shared memory segment located at the address specified
/// by shmaddr from the address space of the calling process
/// @param shmaddr the address of the shared memory segment.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long sys_shmdt(const void *shmaddr);

/// @brief Performs the control operation specified by cmd on the shared memory
/// segment whose identifier is given in shmid.
/// @param shmid the shared memory identifier.
/// @param cmd the command to perform.
/// @param buf is a pointer to a shmid_ds structure.
/// @return a non-negative value basedon on the requested operation, -1 on
/// failure and errno is set to indicate the error.
long sys_shmctl(int shmid, int cmd, struct shmid_ds *buf);

/// @brief Get a System V message queue identifier.
/// @param key can be used either to obtain the identifier of a previously
/// created message queue, or to create a new set.
/// @param msgflg controls the behaviour of the function.
/// @return the message queue identifier, -1 on failure, and errno is set to
/// indicate the error.
int sys_msgget(key_t key, int msgflg);

/// @brief Used to send messages.
/// @param msqid the message queue identifier.
/// @param msgp points to a used-defined msgbuf.
/// @param msgsz specifies the size in bytes of mtext.
/// @param msgflg specifies the action to be taken in case of specific events.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
int sys_msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);

/// @brief Used to receive messages.
/// @param msqid the message queue identifier.
/// @param msgp points to a used-defined msgbuf.
/// @param msgsz specifies the size in bytes of mtext.
/// @param msgtyp specifies the type of message requested, as follows:
/// - msgtyp == 0: the first message on the queue is received.
/// - msgtyp  > 0: the first message of type, msgtyp, is received.
/// - msgtyp  < 0: the first message of the lowest type that is less than or
///                equal to the absolute value of msgtyp is received.
/// @param msgflg specifies the action to be taken in case of specific events.
/// @return the number of bytes actually copied on success, -1 on failure and
/// errno is set to indicate the error.
ssize_t sys_msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);

/// @brief Message queue control operations.
/// @param msqid the message queue identifier.
/// @param cmd The command to perform.
/// @param buf used with IPC_STAT and IPC_SET.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
int sys_msgctl(int msqid, int cmd, struct msqid_ds *buf);

/// @brief creates a new mapping in the virtual address space of the calling process.
/// @param addr the starting address for the new mapping.
/// @param length specifies the length of the mapping (which must be greater than 0).
/// @param prot describes the desired memory protection of the mapping (and must not conflict with the open mode of the file).
/// @param flags determines whether updates to the mapping are visible to other processes mapping the same region.
/// @param fd in case of file mapping, the file descriptor to use.
/// @param offset offset in the file, which must be a multiple of the page size PAGE_SIZE.
/// @return returns a pointer to the mapped area, -1 and errno is set.
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

/// @brief deletes the mappings for the specified address range.
/// @param addr the starting address.
/// @param length the length of the mapped area.
/// @return 0 on success, -1 on falure and errno is set.
int sys_munmap(void *addr, size_t length);

/// @brief Returns system information in the structure pointed to by buf.
/// @param buf Buffer where the info will be placed.
/// @return 0 on success, a negative value on failure.
int sys_uname(utsname_t *buf);

/// @brief System call to create a new pipe.
/// @param fds Array to store read and write file descriptors.
/// @return 0 on success, or -1 on error.
int sys_pipe(int fds[2]);

/// @brief Executes a device-specific control operation on a file descriptor.
/// @param fd The file descriptor for the device or file being operated on.
/// @param request The `ioctl` command, defining the action or configuration.
/// @param data Additional data needed for the `ioctl` command, often a pointer to user-provided data.
/// @return Returns 0 on success; on error, returns a negative error code.
long sys_ioctl(int fd, unsigned int request, unsigned long data);

/// @brief Provides control operations on an open file descriptor.
/// @param fd The file descriptor on which to perform the operation.
/// @param request The `fcntl` command, defining the operation (e.g., `F_GETFL`, `F_SETFL`).
/// @param data Additional data required by certain `fcntl` commands (e.g., flags or pointer).
/// @return Returns 0 on success; on error, returns a negative error code.
long sys_fcntl(int fd, unsigned int request, unsigned long data);
