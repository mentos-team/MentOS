/// @file ext2_attr.c
/// @brief EXT2 file attributes: stat, statfs and setattr.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[EXT2  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "ext2_internal.h"

/// @brief Saves the information concerning the file.
/// @param fs The ext2 filesystem containing the file.
/// @param inode The inode containing the file data.
/// @param inode_index The index of the inode in the filesystem.
/// @param stat The structure where the information is stored.
/// @return 0 on success, or a negative error code on failure.
static int __ext2_stat(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index, stat_t *stat)
{
    stat->st_dev     = fs->block_device->ino;
    stat->st_ino     = inode_index;
    stat->st_mode    = inode->mode;
    stat->st_nlink   = inode->links_count;
    stat->st_uid     = inode->uid;
    stat->st_gid     = inode->gid;
    stat->st_rdev    = 0;
    stat->st_size    = inode->size;
    stat->st_blksize = fs->block_size;
    stat->st_blocks  = inode->blocks_count;
    stat->st_atime   = inode->atime;
    stat->st_mtime   = inode->mtime;
    stat->st_ctime   = inode->ctime;
    return 0;
}

/// @brief Retrieves information concerning the file at the given position.
/// @param file The file struct.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
int ext2_fstat(vfs_file_t *file, stat_t *stat)
{
    if (!file) {
        pr_err("We received a NULL file pointer.\n");
        return -EFAULT;
    }
    if (!stat) {
        pr_err("We received a NULL stat pointer.\n");
        return -EFAULT;
    }
    pr_debug("ext2_fstat(file: %s)\n", file->name);
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", file->name);
        return -EPERM;
    }
    // Get the inode associated with the file.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, file->ino) == -1) {
        pr_err("Failed to read the inode `%s`.\n", file->name);
        return -ENOENT;
    }
    // Set the rest of the structure.
    return __ext2_stat(fs, &inode, file->ino, stat);
}

/// @brief Retrieves information concerning the file at the given position.
/// @param path The path where the file resides.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
int ext2_stat(const char *path, stat_t *stat)
{
    pr_debug("ext2_stat(path: %s)\n", path);
    // Get the EXT2 filesystem.
    ext2_filesystem_t *fs = ext2_get_filesystem(path);
    if (fs == NULL) {
        pr_err("ext2_stat(path: %s): Failed to get the EXT2 filesystem.\n", path);
        return -ENOENT;
    }
    // Prepare the structure for the search.
    ext2_direntry_search_t search;
    memset(&search, 0, sizeof(ext2_direntry_search_t));
    // Resolve the path.
    if (ext2_resolve_path(fs->root, path, &search)) {
        return -ENOENT;
    }
    // Get the inode associated with the directory entry.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, search.direntry.inode) == -1) {
        pr_err("ext2_stat(path: %s): Failed to read the inode of `%s`.\n", path, search.direntry.name);
        return -ENOENT;
    }
    // Set the rest of the structure.
    return __ext2_stat(fs, &inode, search.direntry.inode, stat);
}

int ext2_statfs(const char *path, statfs_t *statfs)
{
    ext2_filesystem_t *fs = ext2_get_filesystem(path);
    if (fs == NULL) {
        pr_err("ext2_statfs(path: %s): Failed to get the EXT2 filesystem.\n", path);
        return -ENOENT;
    }

    if (statfs == NULL) {
        return -EINVAL;
    }

    memset(statfs, 0, sizeof(statfs_t));
    statfs->f_type    = EXT2_SUPERBLOCK_MAGIC;
    statfs->f_bsize   = fs->block_size;
    statfs->f_blocks  = fs->superblock.blocks_count;
    statfs->f_bfree   = fs->superblock.free_blocks_count;
    statfs->f_bavail  = (fs->superblock.free_blocks_count > fs->superblock.r_blocks_count)
                            ? (fs->superblock.free_blocks_count - fs->superblock.r_blocks_count)
                            : 0;
    statfs->f_files   = fs->superblock.inodes_count;
    statfs->f_ffree   = fs->superblock.free_inodes_count;
    statfs->f_namelen = EXT2_NAME_LEN;
    statfs->f_frsize  = fs->block_size;
    return 0;
}

/// @brief Sets the attributes of an inode and saves it
/// @param inode The inode to set the attributes
/// @param attr The structure where the attributes are stored.
/// @return 0 if success.
static int __ext2_setattr(ext2_inode_t *inode, struct iattr *attr)
{
    if (attr->ia_valid & ATTR_MODE) {
        inode->mode = (inode->mode & ~0xfff) | attr->ia_mode;
    }
    if (attr->ia_valid & ATTR_UID) {
        inode->uid = attr->ia_uid;
    }
    if (attr->ia_valid & ATTR_GID) {
        inode->gid = attr->ia_gid;
    }
    if (attr->ia_valid & ATTR_ATIME) {
        inode->atime = attr->ia_atime;
    }
    if (attr->ia_valid & ATTR_MTIME) {
        inode->mtime = attr->ia_mtime;
    }
    if (attr->ia_valid & ATTR_CTIME) {
        inode->ctime = attr->ia_ctime;
    }
    return 0;
}

/// @brief Checks the attributes permission.
/// @param file_owner the file owner we are checking against.
/// @return 1 if it has permission, 0 otherwise.
static int __ext2_check_setattr_permission(uid_t file_owner)
{
    task_struct *task = scheduler_get_current_process();
    return task->uid == 0 || task->uid == file_owner;
}

/// @brief Set attributes of the file at the given position.
/// @param file The file struct.
/// @param attr The structure where the attributes are stored.
/// @return 0 if success.
int ext2_fsetattr(vfs_file_t *file, struct iattr *attr)
{
    pr_debug("ext2_fsetattr(file: %s)\n", file->name);
    if (!__ext2_check_setattr_permission(file->uid)) {
        return -EPERM;
    }
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
    if (fs == NULL) {
        pr_err("fsetattr(%s): The file does not belong to an EXT2 filesystem.\n", file->name);
        return -EPERM;
    }
    // Get the inode associated with the file.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, file->ino) == -1) {
        pr_err("fsetattr(%s): Failed to read the inode.\n", file->name);
        return -ENOENT;
    }
    __ext2_setattr(&inode, attr);
    return ext2_write_inode(fs, &inode, file->ino);
}

/// @brief Set attributes of a file
/// @param path The path where the file resides.
/// @param attr The structure where the information are stored.
/// @return 0 if success.
int ext2_setattr(const char *path, struct iattr *attr)
{
    pr_debug("ext2_setattr(file: %s)\n", path);
    // Get the EXT2 filesystem.
    ext2_filesystem_t *fs = ext2_get_filesystem(path);
    if (fs == NULL) {
        pr_err(
            "setattr(%s): Failed to get the EXT2 filesystem for absolute path "
            "`%s`.\n",
            path);
        return -ENOENT;
    }
    // Prepare the structure for the search.
    ext2_direntry_search_t search;
    memset(&search, 0, sizeof(ext2_direntry_search_t));
    // Resolve the path.
    if (ext2_resolve_path(fs->root, path, &search)) {
        return -ENOENT;
    }
    // Get the inode associated with the directory entry.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, search.direntry.inode) == -1) {
        pr_err("setattr(%s): Failed to read the inode of `%s`.\n", path, search.direntry.name);
        return -ENOENT;
    }
    if (!__ext2_check_setattr_permission(inode.uid)) {
        return -EPERM;
    }
    __ext2_setattr(&inode, attr);
    return ext2_write_inode(fs, &inode, search.direntry.inode);
}
