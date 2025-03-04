/// @file attr.c
/// @brief change file attributes
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "fcntl.h"
#include "fs/namei.h"
#include "fs/vfs.h"
#include "io/debug.h"
#include "libgen.h"
#include "limits.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "stdio.h"
#include "string.h"
#include "system/printk.h"
#include "system/syscall.h"

/// @brief Sets attributes on a file or directory.
///
/// @param path The path of the file or directory whose attributes are to be set.
/// @param attr A pointer to the iattr structure containing the attributes to set.
/// @param follow_links A flag to indicate whether symbolic links should be followed.
/// @return 0 on success, or an appropriate error code on failure.
static int __setattr(const char *path, struct iattr *attr, bool_t follow_links)
{
    // Allocate a variable for the resolved absolute path.
    char absolute_path[PATH_MAX];
    // Resolve the path to its absolute form, optionally following symbolic links.
    int ret = resolve_path(path, absolute_path, sizeof(absolute_path), follow_links ? FOLLOW_LINKS : 0);
    if (ret < 0) {
        pr_err("__setattr(%s): Cannot resolve the absolute path\n", path);
        return ret; // Return the error from resolve_path.
    }
    // Retrieve the superblock for the resolved absolute path.
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("__setattr(%s): Cannot find the superblock!\n", absolute_path);
        return -ENOENT; // Return error if superblock is not found.
    }
    // Retrieve the root of the superblock.
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("__setattr(%s): Cannot find the superblock root!\n", absolute_path);
        return -ENOENT; // Return error if the superblock root is not found.
    }
    // Check if the setattr operation is supported by the filesystem.
    if (sb_root->sys_operations->setattr_f == NULL) {
        pr_err("__setattr(%s): Function not supported in current filesystem\n", absolute_path);
        return -ENOSYS; // Return error if setattr is not implemented.
    }
    // Call the setattr operation with the resolved absolute path and attribute.
    return sb_root->sys_operations->setattr_f(absolute_path, attr);
}

/// @brief Sets the owner and/or group in the iattr structure.
///
/// @param attr A pointer to the iattr structure to update.
/// @param owner The new owner UID to set (use `-1` to leave unchanged).
/// @param group The new group GID to set (use `-1` to leave unchanged).
static inline void __iattr_set_owner_or_group(struct iattr *attr, uid_t owner, gid_t group)
{
    // Set the owner UID if the provided owner is not -1.
    if (owner != (uid_t)-1) {
        attr->ia_valid |= ATTR_UID; // Mark the UID as valid.
        attr->ia_uid = owner;       // Set the UID.
    }
    // Set the group GID if the provided group is not -1.
    if (group != (gid_t)-1) {
        attr->ia_valid |= ATTR_GID; // Mark the GID as valid.
        attr->ia_gid = group;       // Set the GID.
    }
}

int sys_chown(const char *path, uid_t owner, gid_t group)
{
    struct iattr attr = {0};
    __iattr_set_owner_or_group(&attr, owner, group);
    return __setattr(path, &attr, true);
}

int sys_lchown(const char *path, uid_t owner, gid_t group)
{
    struct iattr attr = {0};
    __iattr_set_owner_or_group(&attr, owner, group);
    return __setattr(path, &attr, false);
}

int sys_fchown(int fd, uid_t owner, gid_t group)
{
    task_struct *task = scheduler_get_current_process();

    // Check the current FD.
    if (fd < 0 || fd >= task->max_fd) {
        return -EBADF;
    }

    // Get the file.
    vfs_file_t *file = task->fd_list[fd].file_struct;
    if (file == NULL) {
        return -EBADF;
    }

    if (!(task->uid == 0) || (task->uid == file->uid)) {
        return -EPERM;
    }

    if (owner != -1) {
        file->uid = owner;
    }

    if (group != -1) {
        file->gid = group;
    }

    if (file->fs_operations->setattr_f == NULL) {
        pr_err("No setattr function found for the current filesystem.\n");
        return -ENOSYS;
    }
    struct iattr attr = {0};
    __iattr_set_owner_or_group(&attr, owner, group);
    return file->fs_operations->setattr_f(file, &attr);
}

int sys_chmod(const char *path, mode_t mode)
{
    struct iattr attr = IATTR_CHMOD(mode);
    return __setattr(path, &attr, true);
}

int sys_fchmod(int fd, mode_t mode)
{
    task_struct *task = scheduler_get_current_process();

    // Check the current FD.
    if (fd < 0 || fd >= task->max_fd) {
        return -EBADF;
    }

    // Get the file.
    vfs_file_t *file = task->fd_list[fd].file_struct;
    if (file == NULL) {
        return -EBADF;
    }

    if (!(task->uid == 0) || (task->uid == file->uid)) {
        return -EPERM;
    }

    file->mask &= 0xFFFFFFFF & mode;

    if (file->fs_operations->setattr_f == NULL) {
        pr_err("No setattr function found for the current filesystem.\n");
        return -ENOSYS;
    }
    struct iattr attr = IATTR_CHMOD(mode);
    return file->fs_operations->setattr_f(file, &attr);
}
