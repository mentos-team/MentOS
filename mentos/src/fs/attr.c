/// @file attr.c
/// @brief change file attributes
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "libgen.h"
#include "process/scheduler.h"
#include "fcntl.h"
#include "fs/vfs.h"
#include "io/debug.h"
#include "limits.h"
#include "process/process.h"
#include "stdio.h"
#include "string.h"
#include "sys/errno.h"
#include "system/printk.h"
#include "system/syscall.h"

static int __setattr(const char *path, struct iattr* attr, bool_t follow_links) {
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    if (!realpath(path, absolute_path, sizeof(absolute_path))) {
        pr_err("sys_chown(%s): Cannot get the absolute path.", path);
        return -ENOENT;
    }

    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("do_chown(%s): Cannot find the superblock!\n", absolute_path);
        return -ENOENT;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("do_chown(%s): Cannot find the superblock root!\n", absolute_path);
        return -ENOENT;
    }

    // TODO: resolve links

    // Check if the function is implemented.
    if (sb_root->sys_operations->setattr_f == NULL) {
        pr_err("setattr(%s): Function not supported in current filesystem.", absolute_path);
        return -ENOSYS;
    }
    return sb_root->sys_operations->setattr_f(absolute_path, attr);
}

static inline void __iattr_set_owner_or_group(struct iattr *attr, uid_t owner, gid_t group) {
    if (owner != -1) {
        attr->ia_valid |= ATTR_UID;
        attr->ia_uid = owner;
    }
    if (group != -1) {
        attr->ia_valid |= ATTR_GID;
        attr->ia_gid = group;
    }
}

int sys_chown(const char* path, uid_t owner, gid_t group) {
    struct iattr attr = {0};
    __iattr_set_owner_or_group(&attr, owner, group);
    return __setattr(path, &attr, true);
}

int sys_lchown(const char* path, uid_t owner, gid_t group) {
    struct iattr attr = {0};
    __iattr_set_owner_or_group(&attr, owner, group);
    return __setattr(path, &attr, false);
}

int sys_fchown(int fd, uid_t owner, gid_t group) {
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

int sys_chmod(const char* path, mode_t mode) {
    struct iattr attr = IATTR_CHMOD(mode);
    return __setattr(path, &attr, true);
}

int sys_fchmod(int fd, mode_t mode) {
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
