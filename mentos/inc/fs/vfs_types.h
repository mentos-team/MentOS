/// @file vfs_types.h
/// @brief Virtual filesystem data types.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "list_head.h"
#include "bits/stat.h"
#include "stdint.h"
#include "dirent.h"

#define PATH_SEPARATOR        '/'  ///< The character used as path separator.
#define PATH_SEPARATOR_STRING "/"  ///< The string used as path separator.
#define PATH_UP               ".." ///< The path to the parent.
#define PATH_DOT              "."  ///< The path to the current directory.

/// @brief Data structure containing attributes of a file.
struct iattr {
    /// Validity check on iattr struct.
    unsigned int ia_valid;
    /// Access mode.
    mode_t ia_mode;
    /// Owner uid.
    uid_t ia_uid;
    /// Owner gid.
    gid_t ia_gid;
    /// Time of last access.
    uint32_t ia_atime;
    /// Time of last data modification.
    uint32_t ia_mtime;
    /// Time of last status change.
    uint32_t ia_ctime;
};

/// @brief Filesystem information.
typedef struct file_system_type {
    /// Name of the filesystem.
    const char *name;
    /// Flags of the filesystem.
    int fs_flags;
    /// Mount function.
    struct vfs_file *(*mount)(const char *, const char *);
    /// List head for linking filesystem types.
    struct list_head list;
} file_system_type_t;

/// @brief Set of functions used to perform operations on a filesystem.
typedef struct vfs_sys_operations {
    /// Creates a directory.
    int (*mkdir_f)(const char *, mode_t);
    /// Removes a directory.
    int (*rmdir_f)(const char *);
    /// Retrieves file status information.
    int (*stat_f)(const char *, stat_t *);
    /// Creates a new file or directory.
    struct vfs_file *(*creat_f)(const char *, mode_t);
    /// Creates a symbolic link.
    int (*symlink_f)(const char *, const char *);
    /// Modifies the attributes of a filesystem entry.
    int (*setattr_f)(const char *, struct iattr *);
} vfs_sys_operations_t;

/// @brief Set of functions used to perform operations on files.
typedef struct vfs_file_operations {
    /// Opens a file.
    struct vfs_file *(*open_f)(const char *, int, mode_t);
    /// Removes a file.
    int (*unlink_f)(const char *);
    /// Closes a file.
    int (*close_f)(struct vfs_file *);
    /// Reads data from a file.
    ssize_t (*read_f)(struct vfs_file *, char *, off_t, size_t);
    /// Writes data to a file.
    ssize_t (*write_f)(struct vfs_file *, const void *, off_t, size_t);
    /// Repositions the file offset within a file.
    off_t (*lseek_f)(struct vfs_file *, off_t, int);
    /// Retrieves status information of an open file.
    int (*stat_f)(struct vfs_file *, stat_t *);
    /// Performs an ioctl operation on a file.
    long (*ioctl_f)(struct vfs_file *, unsigned int, unsigned long);
    /// Performs a fcntl operation on a file.
    long (*fcntl_f)(struct vfs_file *, unsigned int, unsigned long);
    /// Reads entries within a directory.
    ssize_t (*getdents_f)(struct vfs_file *, dirent_t *, off_t, size_t);
    /// Reads the target of a symbolic link.
    ssize_t (*readlink_f)(const char *, char *, size_t);
    /// Modifies the attributes of an open file.
    int (*setattr_f)(struct vfs_file *, struct iattr *);
} vfs_file_operations_t;

/// @brief Data structure that contains information about the mounted filesystems.
typedef struct vfs_file {
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
    /// Reference count for this file.
    int32_t refcount;
} vfs_file_t;

/// @brief A structure that represents an instance of a filesystem, i.e., a mounted filesystem.
typedef struct super_block {
    /// Name of the superblock.
    char name[NAME_MAX];
    /// Path of the superblock.
    char path[PATH_MAX];
    /// Pointer to the root file of the given filesystem.
    struct vfs_file *root;
    /// Pointer to the information regarding the filesystem.
    file_system_type_t *type;
    /// List to hold all active mounting points.
    list_head mounts;
} super_block_t;

/// @brief Data structure containing information about an open file.
typedef struct vfs_file_descriptor {
    /// the underlying file structure
    struct vfs_file *file_struct;
    /// Flags for file opening modes.
    int flags_mask;
} vfs_file_descriptor_t;

#define ATTR_MODE  (1 << 0) ///< Flag set to specify the validity of MODE.
#define ATTR_UID   (1 << 1) ///< Flag set to specify the validity of UID.
#define ATTR_GID   (1 << 2) ///< Flag set to specify the validity of GID.
#define ATTR_ATIME (1 << 3) ///< Flag set to specify the validity of ATIME.
#define ATTR_MTIME (1 << 4) ///< Flag set to specify the validity of MTIME.
#define ATTR_CTIME (1 << 5) ///< Flag set to specify the validity of CTIME.

/// Used to initialize an iattr inside the chown function.
#define IATTR_CHOWN(user, group)         \
    {                                    \
        .ia_valid = ATTR_UID | ATTR_GID, \
        .ia_uid   = (user),              \
        .ia_gid   = (group)              \
    }

/// Used to initialize an iattr inside the chmod function.
#define IATTR_CHMOD(mode)      \
    {                          \
        .ia_valid = ATTR_MODE, \
        .ia_mode  = (mode)     \
    }
