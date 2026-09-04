/// @file ext2_namei.c
/// @brief EXT2 names: resolving a path, and creating or removing an entry.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[EXT2  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "ext2_internal.h"

/// @brief Turns the EXT2 file type to OS standard file types.
/// @param ext2_type the EXT2 file type.
/// @return the OS standard file types.
int ext2_file_type_to_vfs_file_type(int ext2_type)
{
    if (ext2_type == ext2_file_type_regular_file) {
        return DT_REG;
    }
    if (ext2_type == ext2_file_type_directory) {
        return DT_DIR;
    }
    if (ext2_type == ext2_file_type_character_device) {
        return DT_CHR;
    }
    if (ext2_type == ext2_file_type_block_device) {
        return DT_BLK;
    }
    if (ext2_type == ext2_file_type_named_pipe) {
        return DT_FIFO;
    }
    if (ext2_type == ext2_file_type_socket) {
        return DT_SOCK;
    }
    if (ext2_type == ext2_file_type_symbolic_link) {
        return DT_LNK;
    }
    return DT_UNKNOWN;
}

/// @brief Checks if the task has x-permission for a given inode
/// @param task the task to check permission for.
/// @param inode the inode to check permission.
/// @return 1 on success, 0 otherwise.
int __valid_x_permission(task_struct *task, ext2_inode_t *inode)
{
    // Init, and all root processes always have permission
    if (!task || (task->pid == 0) || (task->uid == 0)) {
        return 1;
    }

    // Check the owners permission.
    if (task->uid == inode->uid) {
        return inode->mode & S_IXUSR;
    }

    // Check the groups permission.
    if (task->gid == inode->gid) {
        return inode->mode & S_IXGRP;
    }

    // Check the others permission
    return inode->mode & S_IXOTH;
}

/// @brief Searches the entry specified in `path` starting from `directory`.
/// @param directory the directory from which we start performing the search.
/// @param path the path of the entry we are looking for, it can be a relative path.
/// @param search the output variable where we save the entry information.
/// @return 0 on success, -errno on failure.
int ext2_resolve_path(vfs_file_t *directory, const char *path, ext2_direntry_search_t *search)
{
    // Check the pointers.
    if (directory == NULL) {
        pr_err("You provided a NULL directory.\n");
        return -1;
    }
    if (path == NULL) {
        pr_err("You provided a NULL path.\n");
        return -1;
    }
    if (search == NULL) {
        pr_err("You provided a NULL search.\n");
        return -1;
    }
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)directory->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", directory->name);
        return -1;
    }
    // If the path is `/`.
    if (strcmp(path, "/") == 0) {
        return ext2_find_direntry(fs, directory->ino, path, search);
    }
    ino_t ino            = directory->ino;
    char token[NAME_MAX] = {0};
    size_t offset        = 0;
    int tokens;
    while ((tokens = tokenize(path, "/", &offset, token, NAME_MAX)) > 0) {
        if (ext2_find_direntry(fs, ino, token, search)) {
            return -1;
        }
        ino = search->direntry.inode;
    }
    // A component that does not fit the token buffer is rejected, never
    // truncated: a truncated component would resolve to a different name.
    if (tokens < 0) {
        pr_err("Path component too long in `%s`.\n", path);
        return -1;
    }
    pr_debug(
        "ext2_resolve_path(directory: %s, path: %s) -> (%s, %d)\n", directory->name, path, search->direntry.name,
        search->direntry.inode);
    return 0;
}

/// @brief Get the ext2 filesystem object starting from a path.
/// @param absolute_path the absolute path for which we want to find the associated EXT2 filesystem.
/// @return a pointer to the EXT2 filesystem, NULL otherwise.
ext2_filesystem_t *get_ext2_filesystem(const char *absolute_path)
{
    if (absolute_path == NULL) {
        pr_err("We received a NULL absolute path.\n");
        return NULL;
    }
    if (absolute_path[0] != '/') {
        pr_err("We did not received an absolute path `%s`.\n", absolute_path);
        return NULL;
    }
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("Cannot find the superblock for the absolute path `%s`.\n", absolute_path);
        return NULL;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("Cannot find the superblock root for the absolute path `%s`.\n", absolute_path);
        return NULL;
    }
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)sb_root->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", sb_root->name);
        return NULL;
    }
    // Check the magic number.
    if (fs->superblock.magic != EXT2_SUPERBLOCK_MAGIC) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", sb_root->name);
        return NULL;
    }
    return fs;
}

/// @brief Initializes the VFS file.
/// @param fs a pointer to the filesystem.
/// @param file the file we want to initialize.
/// @param inode the inode we use to initialize the VFS file.
/// @param inode_index the inode index.
/// @param name the name of the file.
/// @param name_len the length of the name.
/// @return 0 on success, -errno on failure.
/// @brief Copies into the VFS file the properties that describe the on-disk
///        object, leaving every field that tracks the life of the open file
///        (`count`, `refcount`, `f_pos`, the sibling links) untouched.
/// @param fs a pointer to the filesystem.
/// @param file the file we want to update.
/// @param inode the inode we take the properties from.
/// @param inode_index the inode index.
/// @param name the name of the file.
/// @param name_len the length of the name.
/// @details This is also used to refresh an entry found in the list of open
///          files: that list is keyed by inode number, and a freed inode can
///          be reused by an unrelated file, whose name and permissions would
///          otherwise be those of the file that held the number before it
///          (#297).
void __ext2_set_vfs_file_properties(
    ext2_filesystem_t *fs,
    vfs_file_t *file,
    ext2_inode_t *inode,
    uint32_t inode_index,
    const char *name,
    size_t name_len)
{
    // Copy the name, keeping room for the terminator inside the field.
    size_t copy_len = (name_len < NAME_MAX) ? name_len : (NAME_MAX - 1);
    memcpy(file->name, name, copy_len);
    file->name[copy_len] = 0;
    // Set the device.
    file->device         = (void *)fs;
    // Set the mask.
    file->mask           = inode->mode & 0xFFF;
    // Set UID and GID.
    file->uid            = inode->uid;
    file->gid            = inode->gid;
    // Set the VFS specific flags.
    file->flags          = 0;
    if ((inode->mode & S_IFREG) == S_IFREG) {
        file->flags |= DT_REG;
    }
    if ((inode->mode & S_IFDIR) == S_IFDIR) {
        file->flags |= DT_DIR;
    }
    if ((inode->mode & S_IFBLK) == S_IFBLK) {
        file->flags |= DT_BLK;
    }
    if ((inode->mode & S_IFCHR) == S_IFCHR) {
        file->flags |= DT_CHR;
    }
    if ((inode->mode & S_IFIFO) == S_IFIFO) {
        file->flags |= DT_FIFO;
    }
    if ((inode->mode & S_IFLNK) == S_IFLNK) {
        file->flags |= DT_LNK;
    }
    // Set the inode.
    file->ino            = inode_index;
    // Set the size of the file.
    file->length         = inode->size;
    // Set the timing information.
    file->atime          = inode->atime;
    file->mtime          = inode->mtime;
    file->ctime          = inode->ctime;
    // Set the FS specific operations.
    file->sys_operations = &ext2_sys_operations;
    file->fs_operations  = &ext2_fs_operations;
    // Set the number of links.
    file->nlink          = inode->links_count;
}

/// @brief Initializes the VFS file.
/// @param fs a pointer to the filesystem.
/// @param file the file we want to initialize.
/// @param inode the inode we use to initialize the VFS file.
/// @param inode_index the inode index.
/// @param name the name of the file.
/// @param name_len the length of the name.
/// @return 0 on success, -errno on failure.
int ext2_init_vfs_file(
    ext2_filesystem_t *fs,
    vfs_file_t *file,
    ext2_inode_t *inode,
    uint32_t inode_index,
    const char *name,
    size_t name_len)
{
    // Set everything that comes from the inode.
    __ext2_set_vfs_file_properties(fs, file, inode, inode_index, name, name_len);
    //uint32_t impl;
    //uint32_t open_flags;
    // Set the open count.
    file->count = 0;
    // Set the read offest.
    file->f_pos = 0;
    // Initialize the list of siblings.
    list_head_init(&file->siblings);
    // Set the refcount to zero.
    file->refcount = 0;
    return 0;
}

/// @brief Finds the VFS file that is associated with the given inode index.
/// @param fs a pointer to the fileystem.
/// @param inode the inode index.
/// @return a pointer to the VFS file.
vfs_file_t *ext2_find_vfs_file_with_inode(ext2_filesystem_t *fs, ino_t inode)
{
    vfs_file_t *file = NULL;
    if (!list_head_empty(&fs->opened_files)) {
        list_for_each_decl (it, &fs->opened_files) {
            // Get the file structure.
            file = list_entry(it, vfs_file_t, siblings);
            if (file && (file->ino == inode)) {
                return file;
            }
        }
    }
    return NULL;
}

/// @brief Creates and initializes a new inode.
/// @param fs the filesystem.
/// @param inode the inode we use to initialize the root of the filesystem.
/// @param mode the creat mode.
/// @param preferred_group the preferred group where the inode should be allocated.
/// @return the inode index on success, -1 on failure.
int ext2_create_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, mode_t mode, uint32_t preferred_group)
{
    if (fs == NULL) {
        pr_err("Received a null EXT2 filesystem.\n");
        return -1;
    }
    if (inode == NULL) {
        pr_err("Received a null EXT2 inode.\n");
        return -1;
    }
    // Allocate an inode, inside the preferred_group if possible.
    int inode_index = ext2_allocate_inode(fs, preferred_group);
    if (inode_index == 0) {
        pr_err("Failed to allocate a new inode.\n");
        return -1;
    }
    // Clean the inode structure.
    memset(inode, 0, sizeof(ext2_inode_t));
    // Get the inode associated with the directory entry.
    if (ext2_read_inode(fs, inode, inode_index) == -1) {
        pr_err("Failed to read the newly created inode.\n");
        // The inode is already marked as used on disk: give it back, it owns
        // no block yet, so a zeroed inode is enough to free it (#264).
        ext2_free_inode(fs, inode, inode_index);
        return -1;
    }
    // Get the UID and GID.
    uid_t uid         = 0;
    uid_t gid         = 0;
    task_struct *task = scheduler_get_current_process();
    if (task != NULL) {
        uid = task->uid;
        gid = task->gid;
    }
    // Set the inode mode.
    inode->mode         = mode;
    // Set the user identifiers of the owners.
    inode->uid          = uid;
    // Set the size of the file in bytes.
    inode->size         = 0;
    // Set the time that the inode was accessed.
    inode->atime        = sys_time(NULL);
    // Set the time that the inode was created.
    inode->ctime        = inode->atime;
    // Set the time that the inode was modified the last time.
    inode->mtime        = inode->atime;
    // Set the time that the inode was deleted.
    inode->dtime        = 0;
    // Set the group identifiers of the owners.
    inode->gid          = gid;
    // Set the number of hard links.
    inode->links_count  = 0;
    // Set the blocks count.
    inode->blocks_count = 0;
    // Set the file flags.
    inode->flags        = 0;
    // Set the OS dependant value.
    inode->osd1         = 0;
    // Set the blocks data.
    memset(&inode->data, 0, sizeof(inode->data));
    // Set the value used to indicate the file version (used by NFS).
    inode->generation    = 0;
    // TODO: The value indicating the block number containing the extended attributes.
    inode->file_acl      = 0;
    // TODO: For regular files this 32bit value contains the high 32 bits of the 64bit file size.
    inode->dir_acl       = 0;
    // TODO:Value indicating the location of the file fragment.
    inode->fragment_addr = 0;
    // TODO: OS dependant structure.
    memset(&inode->osd2, 0, sizeof(inode->osd2));
    return inode_index;
}

/// @brief Delete a name and possibly the file it refers to.
/// @param path The path to the file.
/// @return 0 on success, -errno on failure.
int ext2_unlink(const char *path)
{
    pr_debug("ext2_unlink(%s)\n", path);
    int ret          = 0;
    // Get the name of the entry we want to unlink.
    const char *name = basename(path);
    if (name == NULL) {
        pr_err("ext2_unlink(%s): Cannot get the basename.\n", path);
        return -ENOENT;
    }
    // Get the EXT2 filesystem.
    ext2_filesystem_t *fs = get_ext2_filesystem(path);
    if (fs == NULL) {
        pr_err("ext2_unlink(%s): Failed to get the EXT2 filesystem.\n", path);
        return -ENOENT;
    }
    // Prepare the structure for the search.
    ext2_direntry_search_t search;
    memset(&search, 0, sizeof(ext2_direntry_search_t));
    // Resolve the path to the directory entry.
    if (ext2_resolve_path(fs->root, path, &search)) {
        return -ENOENT;
    }
    // Get the inode associated with the parent directory entry.
    ext2_inode_t parent_inode;
    if (ext2_read_inode(fs, &parent_inode, search.parent_inode) == -1) {
        pr_err("ext2_unlink(%s): Failed to read the inode of parent (%d).\n", path, search.parent_inode);
        return -ENOENT;
    }
    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);

    // Read the block where the direntry resides.
    if (ext2_read_inode_block(fs, &parent_inode, search.block_index, cache) == -1) {
        pr_err("ext2_unlink(%s): Failed to read the parent inode block (%d).\n", path, search.block_index);
        ret = -1;
        goto early_exit;
    }
    // Get a pointer to the direntry.
    ext2_dirent_t *actual_dirent = (ext2_dirent_t *)((uintptr_t)cache + search.block_offset);
    if (actual_dirent == NULL) {
        pr_err("ext2_unlink(%s): We found a NULL ext2_dirent_t.\n", path);

        ret = -1;
        goto early_exit;
    }
    // Clear the directory entry.
    actual_dirent->inode = 0;
    memset(actual_dirent->name, 0, actual_dirent->name_len);
    actual_dirent->name_len  = 0;
    actual_dirent->file_type = ext2_file_type_unknown;
    // Write back the parent directory block.
    if (!ext2_write_inode_block(fs, &parent_inode, search.parent_inode, search.block_index, cache)) {
        pr_err("ext2_unlink(%s): Failed to write the inode block (%d).\n", path, search.block_index);
        ret = -1;
        goto early_exit;
    }
    // Read the inode of the direntry we want to unlink.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, search.direntry.inode) == -1) {
        pr_err(
            "ext2_unlink(%s): Failed to read the inode (%d: %s).\n", path, search.direntry.inode, search.direntry.name);
        ret = -1;
        goto early_exit;
    }
    if (--inode.links_count == 0) {
        // Free the inode.
        ext2_free_inode(fs, &inode, search.direntry.inode);
    } else {
        // Update the inode.
        if (ext2_write_inode(fs, &inode, search.direntry.inode) == -1) {
            pr_err(
                "ext2_unlink(%s): Failed to update the inode (%d: %s).\n", path, search.direntry.inode,
                search.direntry.name);
            ret = -1;
            goto early_exit;
        }
    }
early_exit:
    // Free the cache.
    ext2_dealloc_cache(cache);
    return ret;
}

/// @brief Creates a new directory at the given path.
/// @param path The path of the new directory.
/// @param mode The mode with which we create the directory.
/// @return Returns a negative value on failure.
/// @brief Clears the directory entry of an incomplete directory.
/// @param fs the filesystem.
/// @param path the path of the directory being created.
/// @return 0 on success, -1 on failure.
/// @details Used when mkdir cannot finish: the entry it added to the parent
///          must go away before the inode is freed, otherwise the parent
///          keeps a name pointing at a free inode (#264).
int __ext2_clear_direntry_for_path(ext2_filesystem_t *fs, const char *path)
{
    ext2_direntry_search_t search;
    memset(&search, 0, sizeof(ext2_direntry_search_t));
    // Locate the entry we added, so we know which block holds it.
    if (ext2_resolve_path(fs->root, path, &search)) {
        pr_err("Failed to locate the directory entry of `%s`.\n", path);
        return -1;
    }
    ext2_inode_t parent;
    if (ext2_read_inode(fs, &parent, search.parent_inode) == -1) {
        pr_err("Failed to read the parent inode of `%s`.\n", path);
        return -1;
    }
    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);
    // Read the block where the entry resides.
    if (ext2_read_inode_block(fs, &parent, search.block_index, cache) == -1) {
        pr_err("Failed to read block `%u` of inode `%u`.\n", search.block_index, (uint32_t)search.parent_inode);
        ext2_dealloc_cache(cache);
        return -1;
    }
    // An entry pointing at inode 0 is a free entry.
    ext2_dirent_t *dirent = (ext2_dirent_t *)((uintptr_t)cache + search.block_offset);
    dirent->inode         = 0;
    // Write the block back.
    int ret               = 0;
    if (ext2_write_inode_block(fs, &parent, search.parent_inode, search.block_index, cache) == -1) {
        pr_err("Failed to write block `%u` of inode `%u`.\n", search.block_index, (uint32_t)search.parent_inode);
        ret = -1;
    }
    // Free the cache.
    ext2_dealloc_cache(cache);
    return ret;
}

int ext2_mkdir(const char *path, mode_t mode)
{
    pr_debug("ext2_mkdir(path: %s, mode: %u)\n", path, mode);
    // Get the parent directory.
    char parent_path[PATH_MAX];
    if (!dirname(path, parent_path, sizeof(parent_path))) {
        return -ENOENT;
    }
    // Check the parent path.
    if (strcmp(parent_path, path) == 0) {
        pr_err(
            "ext2_mkdir(path: %s): Failed to properly get the parent directory "
            "(%s).\n",
            path, parent_path);
        return -ENOENT;
    }
    // Get the EXT2 filesystem.
    ext2_filesystem_t *fs = get_ext2_filesystem(path);
    if (fs == NULL) {
        pr_err("ext2_mkdir(path: %s): Failed to get the EXT2 filesystem.\n", path);
        return -ENODEV;
    }
    // Prepare the structure for the search.
    ext2_direntry_search_t search;
    // Search if the entry already exists.
    if (!ext2_resolve_path(fs->root, path, &search)) {
        pr_err("ext2_mkdir(path: %s): Directory already exists.\n", path);
        return -EEXIST;
    }
    // Set the inode mode.
    mode = S_IFDIR | (0xFFF & mode);

    // Get the group index of the parent.
    uint32_t group_index = ext2_inode_index_to_group_index(fs, search.parent_inode);
    // Prepare an inode, it will come in handy either way.
    ext2_inode_t inode;
    // Create and initialize the new inode.
    int inode_index = ext2_create_inode(fs, &inode, mode, group_index);
    if (inode_index == -1) {
        pr_err(
            "ext2_mkdir(path: %s): Failed to create a new inode (group index: "
            "%d).\n",
            path, group_index);
        return -ENOSPC;
    }
    // The inode bitmap and the group counters are on disk from here on, so
    // every failure below has to undo them: the directory does not exist for
    // the caller, and nothing of it may survive on the filesystem (#264).
    int ret           = 0;
    int has_direntry  = 0;
    int parent_bumped = 0;
    // Increase the number of directories inside the group.
    fs->block_groups[group_index].used_dirs_count += 1;
    // Write the inode.
    if (ext2_write_inode(fs, &inode, inode_index) == -1) {
        pr_err("Failed to write the newly created inode.\n");
        ret = -EIO;
        goto rollback;
    }
    // Get the directory name.
    const char *directory_name = basename(path);
    // Create a directory entry for the directory.
    if (ext2_allocate_direntry(fs, search.parent_inode, inode_index, directory_name, ext2_file_type_directory) == -1) {
        pr_err(
            "ext2_mkdir(path: %s): Failed to allocate the new direntry `%s` "
            "for the inode.\n",
            path, directory_name);
        ret = -ENOSPC;
        goto rollback;
    }
    has_direntry = 1;
    // Allocate a new block.
    if (!ext2_initialize_new_direntry_block(fs, inode_index, 0)) {
        pr_err(
            "ext2_mkdir(path: %s): Failed to allocate a new block for an "
            "inode.\n",
            path);
        ret = -ENOSPC;
        goto rollback;
    }
    // Create a directory entry, inside the new directory, pointing to itself.
    if (ext2_allocate_direntry(fs, inode_index, inode_index, ".", ext2_file_type_directory) == -1) {
        pr_err(
            "ext2_mkdir(path: %s): Failed to allocate a new direntry for the "
            "inode.\n",
            path);
        ret = -ENOSPC;
        goto rollback;
    }
    // Create a directory entry, inside the new directory, pointing to its parent.
    // This one adds a link to the parent, which the rollback has to remove.
    parent_bumped = 1;
    if (ext2_allocate_direntry(fs, inode_index, search.parent_inode, "..", ext2_file_type_directory) == -1) {
        pr_err(
            "ext2_mkdir(path: %s): Failed to allocate a new direntry for the "
            "inode.\n",
            path);
        ret = -ENOSPC;
        goto rollback;
    }
    return 0;

rollback:
    // The `..` entry counts as a link to the parent, and the link is added
    // before the entry is placed: assume it went through, since the only way
    // it did not is a failed inode write, which already left the parent
    // untouched on disk.
    if (parent_bumped) {
        ext2_inode_t parent_inode;
        if (ext2_read_inode(fs, &parent_inode, search.parent_inode) != -1) {
            if (parent_inode.links_count > 0) {
                parent_inode.links_count -= 1;
                if (ext2_write_inode(fs, &parent_inode, search.parent_inode) == -1) {
                    pr_err("ext2_mkdir(path: %s): Failed to restore the links of the parent.\n", path);
                }
            }
        }
    }
    // Take the name out of the parent before the inode goes away, so no entry
    // is left pointing at a free inode.
    if (has_direntry && (__ext2_clear_direntry_for_path(fs, path) < 0)) {
        pr_err("ext2_mkdir(path: %s): Failed to remove the incomplete directory entry.\n", path);
    }
    // Re-read the inode: initializing its entry block may have given it a
    // data block, and freeing the inode has to free that block as well.
    ext2_inode_t stale;
    if (ext2_read_inode(fs, &stale, inode_index) != -1) {
        stale.links_count = 0;
        stale.dtime       = sys_time(NULL);
        if (ext2_write_inode(fs, &stale, inode_index) == -1) {
            pr_err("ext2_mkdir(path: %s): Failed to update the inode being freed.\n", path);
        }
        if (ext2_free_inode(fs, &stale, inode_index) < 0) {
            pr_err("ext2_mkdir(path: %s): Failed to free the inode.\n", path);
        }
    }
    // Give back the directory count taken above.
    if (fs->block_groups[group_index].used_dirs_count > 0) {
        fs->block_groups[group_index].used_dirs_count -= 1;
    }
    if (ext2_write_bgdt_for_group(fs, group_index) < 0) {
        pr_warning("ext2_mkdir(path: %s): Failed to write BGDT for group %u.\n", path, group_index);
    }
    return ret;
}

/// @brief Removes the given directory.
/// @param path The path to the directory to remove.
/// @return Returns a negative value on failure.
int ext2_rmdir(const char *path)
{
    pr_debug("ext2_rmdir(path: %s)\n", path);
    // Get the name of the entry we want to unlink.
    const char *name = basename(path);
    if (name == NULL) {
        pr_err("ext2_rmdir(path: %s): Cannot get the basename`.\n", path);
        return -ENOENT;
    }
    // Get the EXT2 filesystem.
    ext2_filesystem_t *fs = get_ext2_filesystem(path);
    if (fs == NULL) {
        pr_err("ext2_rmdir(path: %s): Failed to get the EXT2 filesystem.\n", path);
        return -ENOENT;
    }
    // Prepare the structure for the search.
    ext2_direntry_search_t search;
    memset(&search, 0, sizeof(ext2_direntry_search_t));
    // Resolve the path to the directory entry.
    if (ext2_resolve_path(fs->root, path, &search)) {
        return -ENOENT;
    }

    // Get the inode associated with the parent directory entry.
    ext2_inode_t parent;
    if (ext2_read_inode(fs, &parent, search.parent_inode) == -1) {
        pr_err(
            "ext2_rmdir(path: %s): Failed to read the inode of parent of "
            "`%s`.\n",
            path, search.direntry.name);
        return -ENOENT;
    }
    // Read the inode of the direntry we want to unlink.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, search.direntry.inode) == -1) {
        pr_err("ext2_rmdir(path: %s): Failed to read the inode of `%s`.\n", path, search.direntry.name);
        return -ENOENT;
    }
    return ext2_destroy_direntry(
        fs, parent, inode, search.parent_inode, search.direntry.inode, search.block_index, search.block_offset);
}
