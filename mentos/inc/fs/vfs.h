/// @file vfs.h
/// @brief Headers for Virtual File System (VFS).
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "fs/vfs_types.h"
#include "mem/slab.h"

#define MAX_OPEN_FD 16 ///< Maximum number of opened file.

#define STDIN_FILENO  0 ///< Standard input.
#define STDOUT_FILENO 1 ///< Standard output.
#define STDERR_FILENO 2 ///< Standard error output.

extern kmem_cache_t *vfs_file_cache;

/// @brief Forward declaration of task_struct.
struct task_struct;

/// @brief Searches for the mountpoint of the given path.
/// @param absolute_path Path for which we want to search the mountpoint.
/// @return Pointer to the vfs_file of the mountpoint.
super_block_t *vfs_get_superblock(const char *absolute_path);

/// @brief Initialize the Virtual File System (VFS).
void vfs_init(void);

/// @brief Register a new filesystem.
/// @param fs A pointer to the information concerning the new filesystem.
/// @return The outcome of the operation, 0 if fails.
int vfs_register_filesystem(file_system_type *fs);

/// @brief Unregister a new filesystem.
/// @param fs A pointer to the information concerning the filesystem.
/// @return The outcome of the operation, 0 if fails.
int vfs_unregister_filesystem(file_system_type *fs);

/// @brief Given an absolute path to a file, vfs_open_abspath() returns a file struct, used to access the file.
/// @param abspath  An absolute path to a file.
/// @param flags    Used to set the file status flags and file access modes of the open file description.
/// @param mode     Specifies the file mode bits be applied when a new file is created.
/// @return Returns a file struct, or NULL.
vfs_file_t *vfs_open_abspath(const char *pathname, int flags, mode_t mode);

/// @brief Given a pathname for a file, vfs_open() returns a file struct, used to access the file.
/// @param pathname A pathname for a file.
/// @param flags    Used to set the file status flags and file access modes of the open file description.
/// @param mode     Specifies the file mode bits be applied when a new file is created.
/// @return Returns a file struct, or NULL.
vfs_file_t *vfs_open(const char *pathname, int flags, mode_t mode);

/// @brief Decreases the number of references to a given file, if the
///         references number reaches 0, close the file.
/// @param file A pointer to the file structure.
/// @return The result of the call.
int vfs_close(vfs_file_t *file);

/// @brief        Read data from a file.
/// @param file   The file structure used to reference a file.
/// @param buf    The buffer.
/// @param offset The offset from which the function starts to read.
/// @param nbytes The number of bytes to read.
/// @return The number of read characters.
ssize_t vfs_read(vfs_file_t *file, void *buf, size_t offset, size_t nbytes);

/// @brief        Write data to a file.
/// @param file   The file structure used to reference a file.
/// @param buf    The buffer.
/// @param offset The offset from which the function starts to write.
/// @param nbytes The number of bytes to write.
/// @return The number of written characters.
ssize_t vfs_write(vfs_file_t *file, const void *buf, size_t offset, size_t nbytes);

/// @brief Repositions the file offset inside a file.
/// @param file   The file for which we reposition the offest.
/// @param offset The offest to use for the operation.
/// @param whence The type of operation.
/// @return  Upon successful completion, returns the resulting offset
/// location as measured in bytes from the beginning of the file. On
/// error, the value (off_t) -1 is returned and errno is set to
/// indicate the error.
off_t vfs_lseek(vfs_file_t *file, off_t offset, int whence);

/// Provide access to the directory entries.
/// @param file  The directory for which we accessing the entries.
/// @param dirp  The buffer where de data should be placed.
/// @param off   The offset from which we start reading the entries.
/// @param count The size of the buffer.
/// @return On success, the number of bytes read is returned.  On end of
///         directory, 0 is returned.  On error, -1 is returned, and errno is set
///         appropriately.
ssize_t vfs_getdents(vfs_file_t *file, dirent_t *dirp, off_t off, size_t count);

/// @brief Perform the I/O control operation specified by REQUEST on FD.
///   One argument may follow; its presence and type depend on REQUEST.
/// @param file     The file for which we are executing the operations.
/// @param request The device-dependent request code
/// @param data    An untyped pointer to memory.
/// @return Return value depends on REQUEST. Usually -1 indicates error.
int vfs_ioctl(vfs_file_t *file, int request, void *data);

/// @brief Delete a name and possibly the file it refers to.
/// @param path The path to the file.
/// @return On success, zero is returned. On error, -1 is returned, and
/// errno is set appropriately.
int vfs_unlink(const char *path);

/// @brief Creates a new directory at the given path.
/// @param path The path of the new directory.
/// @param mode The permission of the new directory.
/// @return Returns a negative value on failure.
int vfs_mkdir(const char *path, mode_t mode);

/// @brief Removes the given directory.
/// @param path The path to the directory to remove.
/// @return Returns a negative value on failure.
int vfs_rmdir(const char *path);

/// @brief Creates a new file or rewrite an existing one.
/// @param path path to the file.
/// @param mode mode for file creation.
/// @return file descriptor number, -1 otherwise and errno is set to indicate the error.
/// @details
/// It is equivalent to: open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)
vfs_file_t *vfs_creat(const char *path, mode_t mode);

/// @brief Read the symbolic link, if present.
/// @param file the file for which we want to read the symbolic link information.
/// @param buffer the buffer where we will store the symbolic link path.
/// @param bufsize the size of the buffer.
/// @return The number of read characters on success, -1 otherwise and errno is set to indicate the error.
ssize_t vfs_readlink(vfs_file_t *file, char *buffer, size_t bufsize);

/// @brief Creates a symbolic link.
/// @param linkname the name of the link.
/// @param path the entity it is linking to.
/// @return 0 on success, a negative number if fails and errno is set.
int vfs_symlink(const char *linkname, const char *path);

/// @brief Stat the file at the given path.
/// @param path Path to the file for which we are retrieving the statistics.
/// @param buf  Buffer where we are storing the statistics.
/// @return 0 on success, a negative number if fails and errno is set.
int vfs_stat(const char *path, stat_t *buf);

/// @brief Stat the given file.
/// @param file Pointer to the file for which we are retrieving the statistics.
/// @param buf  Buffer where we are storing the statistics.
/// @return 0 on success, a negative number if fails and errno is set.
int vfs_fstat(vfs_file_t *file, stat_t *buf);

/// @brief Mount a file system to the specified path.
/// @param path    Path where we want to map the filesystem.
/// @param fs_root Root node of the filesystem.
/// @return 1 on success, 0 on fail.
/// @details
/// For example, if we have an EXT2 filesystem with a root node
/// of ext2_root and we want to mount it to /, we would run
/// vfs_mount("/", ext2_root); - or, if we have a procfs node,
/// we could mount that to /dev/procfs. Individual files can also
/// be mounted.
int vfs_mount(const char *path, vfs_file_t *fs_root);

/// @brief Mount the path as a filesystem of the given type.
/// @param type The type of filesystem
/// @param path The path to where it should be mounter.
/// @param args The arguments passed to the filesystem mount callback.
/// @return 0 on success, a negative number if fails and errno is set.
int do_mount(const char *type, const char *path, const char *args);

/// @brief Locks the access to the given file.
/// @param file The file to lock.
void vfs_lock(vfs_file_t *file);

/// @brief Extends the file descriptor list for the given task.
/// @param task The task for which we extend the file descriptor list.
/// @return 0 on fail, 1 on success.
int vfs_extend_task_fd_list(struct task_struct *task);

/// @brief Initialize the file descriptor list for the given task.
/// @param task The task for which we initialize the file descriptor list.
/// @return 0 on fail, 1 on success.
int vfs_init_task(struct task_struct *task);

/// @brief Duplicate the file descriptor list of old_task into new_task.
/// @param new_task The task where we clone the file descriptor list.
/// @param old_task The task from which we clone the file descriptor list.
/// @return 0 on fail, 1 on success.
int vfs_dup_task(struct task_struct *new_task, struct task_struct *old_task);

/// @brief Destroy the file descriptor list for the given task.
/// @param task The task for which we destroy the file descriptor list.
/// @return 0 on fail, 1 on success.
int vfs_destroy_task(struct task_struct *task);

/// @brief Find the smallest available fd.
/// @return -errno on fail, fd on success.
int get_unused_fd(void);

/// @brief Return new smallest available file desriptor.
/// @return -errno on fail, fd on success.
int sys_dup(int fd);

/// @brief Check if the requested open flags against the file mask
/// @param flags The requested open flags.
/// @param mode The permissions of the file.
/// @param uid The owner of the task opening the file.
/// @param gid The group of the task opening the file.
/// @return 0 on fail, 1 on success.
int vfs_valid_open_permissions(int flags, mode_t mask, uid_t uid, gid_t gid);

/// @brief Check if the file is exectuable
/// @param task The task to execute the file.
/// @param file The file to execute.
/// @return 0 on fail, 1 on success.
int vfs_valid_exec_permission(struct task_struct *task, vfs_file_t *file);
