/// @file vfs_types.h
/// @brief Virtual filesystem data types.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/list_head.h"
#include "sys/dirent.h"
#include "bits/stat.h"
#include "stdint.h"

#define PATH_SEPARATOR        '/'  ///< The character used as path separator.
#define PATH_SEPARATOR_STRING "/"  ///< The string used as path separator.
#define PATH_UP               ".." ///< The path to the parent.
#define PATH_DOT              "."  ///< The path to the current directory.

/// Forward declaration of the VFS file.
typedef struct vfs_file_t vfs_file_t;

/// Forward declaration of the inode attributes.
struct iattr;

/// Function used to create a directory.
typedef int (*vfs_mkdir_callback)(const char *, mode_t);
/// Function used to remove a directory.
typedef int (*vfs_rmdir_callback)(const char *);
/// Function used to open a file (or directory).
typedef vfs_file_t *(*vfs_creat_callback)(const char *, mode_t);
/// Function used to read the entries of a directory.
typedef ssize_t (*vfs_getdents_callback)(vfs_file_t *, dirent_t *, off_t, size_t);
/// Function used to open a file (or directory).
typedef vfs_file_t *(*vfs_open_callback)(const char *, int, mode_t);
/// Function used to remove a file.
typedef int (*vfs_unlink_callback)(const char *);
/// Function used to close a file.
typedef int (*vfs_close_callback)(vfs_file_t *);
/// Function used to read from a file.
typedef ssize_t (*vfs_read_callback)(vfs_file_t *, char *, off_t, size_t);
/// Function used to write inside a file.
typedef ssize_t (*vfs_write_callback)(vfs_file_t *, const void *, off_t, size_t);
/// Function used to reposition the file offset inside a file.
typedef off_t (*vfs_lseek_callback)(vfs_file_t *, off_t, int);
/// Function used to stat fs entries.
typedef int (*vfs_stat_callback)(const char *, stat_t *);
/// Function used to stat files.
typedef int (*vfs_fstat_callback)(vfs_file_t *, stat_t *);
/// Function used to perform ioctl on files.
typedef int (*vfs_ioctl_callback)(vfs_file_t *, int, void *);
/// Function for creating symbolic links.
typedef int (*vfs_symlink_callback)(const char *, const char *);
/// Function that reads the symbolic link data associated with a file.
typedef ssize_t (*vfs_readlink_callback)(vfs_file_t *, char *, size_t);
/// Function used to modify the attributes of an fs entry.
typedef int (*vfs_setattr_callback)(const char *, struct iattr *);
/// Function used to modify the attributes of a file.
typedef int (*vfs_fsetattr_callback)(vfs_file_t *, struct iattr *);

/// @brief Filesystem information.
typedef struct file_system_type {
    /// Name of the filesystem.
    const char *name;
    /// Flags of the filesystem.
    int fs_flags;
    /// Mount function.
    vfs_file_t *(*mount)(const char *, const char *);
} file_system_type;

/// @brief Set of functions used to perform operations on filesystem.
typedef struct vfs_sys_operations_t {
    /// Creates a directory.
    vfs_mkdir_callback mkdir_f;
    /// Removes a directory.
    vfs_rmdir_callback rmdir_f;
    /// Stat function.
    vfs_stat_callback stat_f;
    /// File creation function.
    vfs_creat_callback creat_f;
    /// Symbolic link creation function.
    vfs_symlink_callback symlink_f;
    /// Modifies the attributes of a file.
    vfs_setattr_callback setattr_f;
} vfs_sys_operations_t;

/// @brief Set of functions used to perform operations on files.
typedef struct vfs_file_operations_t {
    /// Open a file.
    vfs_open_callback open_f;
    /// Remove a file.
    vfs_unlink_callback unlink_f;
    /// Close a file.
    vfs_close_callback close_f;
    /// Read from a file.
    vfs_read_callback read_f;
    /// Write inside a file.
    vfs_write_callback write_f;
    /// Reposition the file offset inside a file.
    vfs_lseek_callback lseek_f;
    /// Stat the file.
    vfs_fstat_callback stat_f;
    /// Perform ioctl on file.
    vfs_ioctl_callback ioctl_f;
    /// Read entries inside the directory.
    vfs_getdents_callback getdents_f;
    /// Reads the symbolik link data.
    vfs_readlink_callback readlink_f;
    /// Modifies the attributes of a file.
    vfs_fsetattr_callback setattr_f;
} vfs_file_operations_t;

/// @brief Data structure that contains information about the mounted filesystems.
struct vfs_file_t {
    /// The filename.
    char name[NAME_MAX];
    /// Device object (optional).
    void *device;
    /// The permissions mask.
    uint32_t mask;
    /// The owning user.
    uint32_t uid;
    /// The owning group.
    uint32_t gid;
    /// Flags (node type, etc).
    uint32_t flags;
    /// Inode number.
    uint32_t ino;
    /// Size of the file, in byte.
    uint32_t length;
    /// Used to keep track which fs it belongs to.
    uint32_t impl;
    /// Flags passed to open (read/write/append, etc.)
    uint32_t open_flags;
    /// Number of file descriptors associated with this file
    int count;
    /// Accessed (time).
    uint32_t atime;
    /// Modified (time).
    uint32_t mtime;
    /// Created (time).
    uint32_t ctime;
    /// Generic system operations.
    vfs_sys_operations_t *sys_operations;
    /// Files operations.
    vfs_file_operations_t *fs_operations;
    /// Offset for read operations.
    size_t f_pos;
    /// The number of links.
    uint32_t nlink;
    /// List to hold all active files associated with a specific entry in a filesystem.
    list_head siblings;
    /// TODO: Comment.
    int32_t refcount;
};

/// @brief A structure that represents an instance of a filesystem, i.e., a mounted filesystem.
typedef struct super_block_t {
    /// Name of the superblock.
    char name[NAME_MAX];
    /// Path of the superblock.
    char path[PATH_MAX];
    /// Pointer to the root file of the given filesystem.
    vfs_file_t *root;
    /// Pointer to the information regarding the filesystem.
    file_system_type *type;
    /// List to hold all active mounting points.
    list_head mounts;
} super_block_t;

/// @brief Data structure containing information about an open file.
typedef struct vfs_file_descriptor_t {
    /// the underlying file structure
    vfs_file_t *file_struct;
    /// Flags for file opening modes.
    int flags_mask;
} vfs_file_descriptor_t;

/// @brief Data structure containing attributes of a file.
struct iattr {
    unsigned int ia_valid;
    mode_t ia_mode;
    uid_t ia_uid;
    gid_t ia_gid;
    uint32_t ia_atime;
    uint32_t ia_mtime;
    uint32_t ia_ctime;
};

#define ATTR_MODE  (1 << 0)
#define ATTR_UID   (1 << 1)
#define ATTR_GID   (1 << 2)
#define ATTR_ATIME (1 << 3)
#define ATTR_MTIME (1 << 4)
#define ATTR_CTIME (1 << 5)

#define IATTR_CHOWN(user, group)       \
    { .ia_valid = ATTR_UID | ATTR_GID, \
      .ia_uid = user,                  \
      .ia_gid = group }

#define IATTR_CHMOD(mode)    \
    { .ia_valid = ATTR_MODE, \
      .ia_mode = mode }
