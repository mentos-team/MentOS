/// @file attr.h
/// @brief Change file attributes.
/// @details This header provides declarations for system calls that handle file
/// ownership and permissions, such as changing the owner, group, and mode of
/// files or file descriptors.
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stddef.h"

/// @brief Change the owner and group of a file by path. It modifies the file's
/// ownership attributes to the provided `owner` and `group`.
///
/// @param path Path to the file.
/// @param owner The user ID of the new owner.
/// @param group The group ID of the new group.
/// @return 0 on success, or -1 on error.
int sys_chown(const char *path, uid_t owner, gid_t group);

/// @brief Change the owner and group of a symbolic link. It modifies the
/// symbolic link's ownership attributes to the provided `owner` and `group`.
/// This does not follow symbolic links, modifying only the link itself.
///
/// @param path Path to the symbolic link.
/// @param owner The user ID of the new owner.
/// @param group The group ID of the new group.
/// @return 0 on success, or -1 on error.
int sys_lchown(const char *path, uid_t owner, gid_t group);

/// @brief Change the owner and group of a file by file descriptor. It modifies
/// the file's ownership attributes to the provided `owner` and `group`.
///
/// @param fd File descriptor referring to the file.
/// @param owner The user ID of the new owner.
/// @param group The group ID of the new group.
/// @return 0 on success, or -1 on error.
int sys_fchown(int fd, uid_t owner, gid_t group);

/// @brief Change the mode (permissions) of a file by path. It modifies the
/// file's permissions to the provided `mode`, determining the file's access
/// permissions.
///
/// @param path Path to the file.
/// @param mode The new file mode (permissions).
/// @return 0 on success, or -1 on error.
int sys_chmod(const char *path, mode_t mode);

/// @brief Change the mode (permissions) of a file by file descriptor. It
/// modifies the file's permissions to the provided `mode`, determining the
/// file's access permissions.
///
/// @param fd File descriptor referring to the file.
/// @param mode The new file mode (permissions).
/// @return 0 on success, or -1 on error.
int sys_fchmod(int fd, mode_t mode);
