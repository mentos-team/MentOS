/// @file ext2_dir.c
/// @brief EXT2 directory entries: iteration, creation and removal.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[EXT2  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "ext2_internal.h"

/// @brief Returns the ext2_dirent_t pointed by the iterator.
/// @param it the iterator.
/// @return pointer to the ext2_dirent_t
ext2_dirent_t *ext2_direntry_iterator_get(ext2_direntry_iterator_t *it)
{
    return (ext2_dirent_t *)((uintptr_t)it->cache + it->block_offset);
}

/// @brief Check if the iterator is valid.
/// @param it the iterator to check.
/// @return 1 if valid, 0 otherwise.
int ext2_direntry_iterator_valid(ext2_direntry_iterator_t *it) { return it->direntry != NULL; }

/// @brief Initializes the iterator and reads the first block.
/// @param fs pointer to the filesystem.
/// @param cache used for reading.
/// @param inode pointer to the directory inode.
/// @return The initialized directory iterator.
ext2_direntry_iterator_t ext2_direntry_iterator_begin(ext2_filesystem_t *fs, uint8_t *cache, ext2_inode_t *inode)
{
    ext2_direntry_iterator_t it = {
        .fs           = fs,
        .cache        = cache,
        .inode        = inode,
        .block_index  = 0,
        .total_offset = 0,
        .block_offset = 0,
        .direntry     = NULL};
    // Start by reading the first block of the inode.
    if (ext2_read_inode_block(fs, inode, it.block_index, cache) == -1) {
        pr_err("Failed to read the inode block `%d`\n", it.block_index);
    } else {
        // Initialize the directory entry.
        it.direntry = ext2_direntry_iterator_get(&it);
    }
    return it;
}

/// @brief Moves to the next direntry, and moves to the next block if necessary.
/// @param it the iterator.
void ext2_direntry_iterator_next(ext2_direntry_iterator_t *it)
{
    uint32_t rec_len = it->direntry->rec_len;
    // A record length of zero leaves the offsets where they are, so the walk
    // never reaches the end of the directory and never returns: one lookup on
    // a directory whose block reads as zeros would hang the kernel, which is
    // not preemptible. A length that is not a multiple of four, or one that
    // runs past the end of the block, cannot be walked either. Stop the walk
    // instead, and let the caller report that the entry was not found (#304).
    if ((rec_len == 0) || ((rec_len % 4) != 0) || ((it->block_offset + rec_len) > it->fs->block_size)) {
        pr_err(
            "Corrupt directory entry: rec_len %u at offset %u of block %u.\n", rec_len, it->block_offset,
            it->block_index);
        it->direntry = NULL;
        return;
    }
    // Advance the offsets.
    it->block_offset += rec_len;
    it->total_offset += rec_len;
    // If we reached the end of the inode, stop.
    if (it->total_offset >= it->inode->size) {
        // The iterator is not valid anymore.
        it->direntry = NULL;
        return;
    }
    // If we exceed the size of a block, move to the next block.
    if (it->block_offset >= it->fs->block_size) {
        // Increase the block index, and reset the block offset.
        it->block_index += 1, it->block_offset = 0;
        // Read the new block.
        if (ext2_read_inode_block(it->fs, it->inode, it->block_index, it->cache) == -1) {
            pr_err("Failed to read the inode block `%d`.\n", it->block_index);
            // The iterator is not valid anymore.
            it->direntry = NULL;
            return;
        }
    }
    // Read the direntry.
    it->direntry = ext2_direntry_iterator_get(it);
}

/// @brief Checks if the directory is empty.
/// @param fs a pointer to the filesystem.
/// @param cache used for reading.
/// @param inode the inode of the directory.
/// @return 1 if empty, 0 if not empty.
static inline int ext2_directory_is_empty(ext2_filesystem_t *fs, uint8_t *cache, ext2_inode_t *inode)
{
    ext2_direntry_iterator_t it = ext2_direntry_iterator_begin(fs, cache, inode);
    for (; ext2_direntry_iterator_valid(&it); ext2_direntry_iterator_next(&it)) {
        if (!strncmp(it.direntry->name, ".", 1) || !strncmp(it.direntry->name, "..", 2)) {
            continue;
        }
        if (it.direntry->inode != 0) {
            return 0;
        }
    }
    return 1;
}

/// @brief Initializes a directory entry.
/// @param direntry a pointer to the directory entry we want to initialize.
/// @param name the name of the new entry.
/// @param inode_index its inode index.
/// @param rec_len the length of the new entry.
/// @param file_type its file type.
static inline void ext2_initialize_direntry(
    ext2_dirent_t *direntry,
    const char *name,
    ino_t inode_index,
    uint32_t rec_len,
    uint8_t file_type)
{
    // Initialize the new directory entry.
    direntry->inode     = inode_index;
    direntry->rec_len   = rec_len;
    direntry->name_len  = strlen(name);
    direntry->file_type = file_type;
    memset(direntry->name, 0, direntry->name_len + 1);
    strncpy(direntry->name, name, direntry->name_len);
}

/// @brief Initializes a new directory entry block for the specified inode.
/// @param fs Pointer to the ext2 filesystem structure.
/// @param inode_index The index of the inode for which the directory entry block is to be initialized.
/// @param block_index The index of the block to be allocated.
/// @return 1 on success, 0 on failure.
int ext2_initialize_new_direntry_block(ext2_filesystem_t *fs, uint32_t inode_index, uint32_t block_index)
{
    ext2_inode_t inode;

    // Error check: Ensure the filesystem pointer is valid
    if (!fs) {
        pr_err("Invalid filesystem pointer.\n");
        return 0;
    }

    // Read the inode for the specified inode index
    if (ext2_read_inode(fs, &inode, inode_index) == -1) {
        pr_err("Failed to read the inode of `%u`.\n", inode_index);
        return 0;
    }

    // Allocate a new block for the inode
    if (ext2_allocate_inode_block(fs, &inode, inode_index, block_index) == -1) {
        pr_err("Failed to allocate a new block for inode `%u`.\n", inode_index);
        return 0;
    }

    // Update the inode size based on the new block index
    inode.size = (block_index + 1) * fs->block_size;

    // Write the updated inode back to the filesystem.
    // Note: ext2_allocate_inode_block already wrote the inode, but we write it again here
    // because we updated the size field after that call. This is necessary for correctness.
    if (ext2_write_inode(fs, &inode, inode_index) == -1) {
        pr_err("Failed to update the inode of `%u`.\n", inode_index);
        return 0;
    }

    // Allocate memory for the cache
    uint8_t *cache = ext2_alloc_cache(fs);

    // Get the first uninitialized directory entry in the block
    ext2_dirent_t *direntry = (ext2_dirent_t *)cache;

    // Initialize the new directory entry with an empty name and unknown file type
    ext2_initialize_direntry(direntry, "", 0, fs->block_size, ext2_file_type_unknown);

    // Write the updated block (with new directory entry) back to the filesystem
    if (ext2_write_inode_block(fs, &inode, inode_index, block_index, cache) == -1) {
        pr_err("Failed to write the block for inode `%u`.\n", inode_index);
        ext2_dealloc_cache(cache); // Free allocated cache memory before returning
        return 0;
    }

    // Free the allocated cache memory after usage
    ext2_dealloc_cache(cache);

    // Return success
    return 1;
}

/// @brief Searches for a free unused directory entry (inode == 0).
/// @param fs a pointer to the filesystem.
/// @param cache cache for memory EXT2 operations.
/// @param parent_inode_index the parent inode index.
/// @param name the name of the new entry.
/// @param inode_index its inode index.
/// @param file_type its file type.
/// @return 1 on success, 0 on failure.
static inline int ext2_get_free_direntry(
    ext2_filesystem_t *fs,
    uint8_t *cache,
    ino_t parent_inode_index,
    const char *name,
    ino_t inode_index,
    uint8_t file_type)
{
    // Read the parent inode.
    ext2_inode_t parent_inode;
    if (ext2_read_inode(fs, &parent_inode, parent_inode_index) == -1) {
        pr_err("Failed to read the parent inode `%u`.\n", parent_inode_index);
        return 0;
    }
    // Get the rec_len;
    uint32_t rec_len            = ext2_get_rec_len_from_name(name);
    // Prepare iterator.
    ext2_direntry_iterator_t it = ext2_direntry_iterator_begin(fs, cache, &parent_inode);
    // Iterate the directory entries.
    for (; ext2_direntry_iterator_valid(&it); ext2_direntry_iterator_next(&it)) {
        // If we hit a direntry with an empty inode, that is a free direntry.
        // Then, we check that the rec_len of the free direntry is big enough.
        if ((it.direntry->inode == 0) && (rec_len <= it.direntry->rec_len)) {
            // Initialize the new directory entry.
            if (ext2_is_last_directory_entry(it.direntry)) {
                assert(fs->block_size > it.block_offset);
                rec_len = fs->block_size - it.block_offset;
            }
            ext2_initialize_direntry(it.direntry, name, inode_index, rec_len, file_type);
            // Update the inode block.
            if (ext2_write_inode_block(fs, &parent_inode, parent_inode_index, it.block_index, cache) == -1) {
                pr_err("Failed to update the block of the father directory.\n");
                return 0;
            }
            pr_debug("Found free directory entry:\n");
            ext2_dump_dirent(it.direntry);
            return 1;
        }
    }
    return 0;
}

/// @brief Appends the new directory entry at the end of the last used block, if
/// there is enough space.
/// @param fs a pointer to the filesystem.
/// @param cache cache for memory EXT2 operations.
/// @param parent_inode_index the parent inode index.
/// @param name the name of the new entry.
/// @param inode_index its inode index.
/// @param file_type its file type.
/// @return 1 on success, 0 on failure.
static inline int ext2_append_new_direntry(
    ext2_filesystem_t *fs,
    uint8_t *cache,
    ino_t parent_inode_index,
    const char *name,
    ino_t inode_index,
    uint8_t file_type)
{
    // Read the parent inode.
    ext2_inode_t parent_inode;
    if (ext2_read_inode(fs, &parent_inode, parent_inode_index) == -1) {
        pr_err("Failed to read the parent inode `%u`.\n", parent_inode_index);
        return 0;
    }
    // Get the rec_len;
    uint32_t rec_len = ext2_get_rec_len_from_name(name);
    uint32_t real_rec_len;
    // Prepare iterator.
    ext2_direntry_iterator_t it = ext2_direntry_iterator_begin(fs, cache, &parent_inode);
    // Iterate the directory entries.
    for (; ext2_direntry_iterator_valid(&it); ext2_direntry_iterator_next(&it)) {
        // Check if we reached the last directory entry, if that's the case, we
        // check if the remaining space is big enough.
        if (ext2_is_last_directory_entry(it.direntry) && ((it.block_offset + rec_len) <= fs->block_size)) {
            pr_debug(
                "Found last directory entry (offset: %u, %u != "
                "round(%u+%u+1):\n",
                it.block_offset, it.direntry->rec_len, sizeof(ext2_dirent_t), it.direntry->name_len);

            ext2_dump_dirent(it.direntry);
            // Compute the real rec_len of the entry.
            real_rec_len         = ext2_get_rec_len_from_direntry(it.direntry);
            // Fix the rec_len of the entry.
            it.direntry->rec_len = real_rec_len;
            // Move the block offsets correctly.
            it.block_offset += real_rec_len;
            it.total_offset += real_rec_len;
            // Set the iterator pointer to the new free location.
            it.direntry = ext2_direntry_iterator_get(&it);
            // Initialize the new directory entry.
            ext2_initialize_direntry(it.direntry, name, inode_index, fs->block_size - it.block_offset, file_type);
            pr_debug(
                "Appended new directory entry (offset: %u -> %u):\n", it.block_offset - real_rec_len, it.block_offset);
            ext2_dump_dirent(it.direntry);
            // Update the inode block.
            if (ext2_write_inode_block(fs, &parent_inode, parent_inode_index, it.block_index, cache) == -1) {
                pr_err("Failed to update the block of the father directory.\n");
                return 0;
            }
            return 1;
        }
    }
    return 0;
}

/// @brief Allocates a new block, and creates a new directory entry inside that
/// new block.
/// @param fs a pointer to the filesystem.
/// @param cache cache for memory EXT2 operations.
/// @param parent_inode_index the parent inode index.
/// @param name the name of the new entry.
/// @param inode_index its inode index.
/// @param file_type its file type.
/// @return 1 on success, 0 on failure.
static inline int ext2_create_new_direntry(
    ext2_filesystem_t *fs,
    uint8_t *cache,
    ino_t parent_inode_index,
    const char *name,
    ino_t inode_index,
    uint8_t file_type)
{
    // Read the parent inode.
    ext2_inode_t parent_inode;
    if (ext2_read_inode(fs, &parent_inode, parent_inode_index) == -1) {
        pr_err("Failed to read the parent inode `%u`.\n", parent_inode_index);
        return 0;
    }
    // The new block goes after the ones the directory already has. Walking
    // the entries to find it does not work: the iterator stops as soon as it
    // reaches the size of the directory, so its block index is the last block
    // in use rather than the first free one, and allocating there replaced
    // the block holding every existing entry (#309).
    uint32_t old_size    = parent_inode.size;
    uint32_t block_index = parent_inode.size / fs->block_size;
    uint32_t real_index  = 0;

    // Allocate the new block and map it at that index.
    if (ext2_allocate_inode_block(fs, &parent_inode, parent_inode_index, block_index) == -1) {
        pr_err("Failed to allocate a new block for an inode.\n");
        return 0;
    }
    // The directory now covers one block more.
    parent_inode.size = (block_index + 1) * fs->block_size;
    if (ext2_write_inode(fs, &parent_inode, parent_inode_index) == -1) {
        pr_err("Failed to update the inode of the father directory.\n");
        goto free_block_and_fail;
    }
    // The new block holds one entry, spanning the whole block: the next
    // append splits it, the same way the first block of a directory works.
    memset(cache, 0, fs->block_size);
    ext2_initialize_direntry((ext2_dirent_t *)cache, name, inode_index, fs->block_size, file_type);
    // Write the new block.
    if (ext2_write_inode_block(fs, &parent_inode, parent_inode_index, block_index, cache) == -1) {
        pr_err("Failed to update the block of the father directory.\n");
        goto free_block_and_fail;
    }
    pr_debug("Created new directory entry:\n");
    ext2_dump_dirent((ext2_dirent_t *)cache);
    return 1;

free_block_and_fail:
    // Give the block back and leave the directory the size it had, so a
    // failed append changes nothing.
    real_index = ext2_get_real_block_index(fs, &parent_inode, block_index);
    if (real_index != 0) {
        ext2_free_block(fs, real_index);
    }
    parent_inode.size = old_size;
    if (ext2_write_inode(fs, &parent_inode, parent_inode_index) == -1) {
        pr_err("Failed to restore the inode of the father directory.\n");
    }
    return 0;
}

/// @brief Allocates a directory entry.
/// @param fs a pointer to the filesystem.
/// @param parent_inode_index the inode index of the parent.
/// @param direntry_inode_index the inode index of the new entry.
/// @param name the name of the new entry.
/// @param file_type the type of file.
/// @return 0 on success, a negative value on failure.
int ext2_allocate_direntry(
    ext2_filesystem_t *fs,
    uint32_t parent_inode_index,
    uint32_t direntry_inode_index,
    const char *name,
    uint8_t file_type)
{
    // Get the inode associated with the new directory entry.
    ext2_inode_t direntry_inode;
    if (ext2_read_inode(fs, &direntry_inode, direntry_inode_index) == -1) {
        pr_err("Failed to read the inode of the directory entry (%d).\n", direntry_inode_index);
        return -1;
    }
    // Update the number of links to the inode.
    direntry_inode.links_count += 1;
    // Write the inode back.
    if (ext2_write_inode(fs, &direntry_inode, direntry_inode_index) == -1) {
        pr_err("Failed to update the inode of the directory entry (%d).\n", direntry_inode_index);
        return -1;
    }
    // Get the inode associated with the parent directory.
    ext2_inode_t parent_inode;
    if (ext2_read_inode(fs, &parent_inode, parent_inode_index) == -1) {
        pr_err("Failed to read the parent inode (%d).\n", parent_inode_index);
        return -1;
    }
    // Check that the parent is a directory.
    if (!bitmask_check(parent_inode.mode, S_IFDIR)) {
        pr_err("The parent inode is not a directory (ino: %d, mode: %d).\n", parent_inode_index, parent_inode.mode);
        return -1;
    }
    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);

    // pr_debug("BEFORE:\n");
    // ext2_dump_direntries(fs, cache, &parent_inode);
    // pr_debug("\n");

    // Get free directory entry.
    if (!ext2_get_free_direntry(fs, cache, parent_inode_index, name, direntry_inode_index, file_type)) {
        if (!ext2_append_new_direntry(fs, cache, parent_inode_index, name, direntry_inode_index, file_type)) {
            if (!ext2_create_new_direntry(fs, cache, parent_inode_index, name, direntry_inode_index, file_type)) {
                pr_err("Failed to place directory entry.\n");
                // Free the cache.
                ext2_dealloc_cache(cache);
                return -1;
            }
        }
    }

    // pr_debug("AFTER:\n");
    // ext2_dump_direntries(fs, cache, &parent_inode);
    // pr_debug("\n");

    // Free the cache.
    ext2_dealloc_cache(cache);
    return 0;
}

/// @brief Destroys a directory entry in the parent directory.
/// @param fs A pointer to the filesystem structure.
/// @param parent The inode of the parent directory.
/// @param inode The inode of the directory entry to be destroyed.
/// @param parent_index The index of the parent inode.
/// @param inode_index The index of the directory entry's inode.
/// @param block_index The block index where the directory entry resides.
/// @param block_offset The offset within the block where the directory entry is located.
/// @return 0 on success, a negative value on failure.
int ext2_destroy_direntry(
    ext2_filesystem_t *fs,
    ext2_inode_t parent,
    ext2_inode_t inode,
    uint32_t parent_index,
    uint32_t inode_index,
    uint32_t block_index,
    uint32_t block_offset)
{
    pr_debug("destroy_direntry(parent: %u, entry: %u)\n", parent_index, inode_index);

    // Validate filesystem pointer
    if (!fs) {
        pr_err("Invalid filesystem pointer.\n");
        return -EINVAL;
    }

    // Validate inode indices
    if (parent_index == 0 || inode_index == 0) {
        pr_err("Invalid inode index (parent: %u, inode: %u).\n", parent_index, inode_index);
        return -EINVAL;
    }

    // Allocate and clean the cache
    uint8_t *cache = ext2_alloc_cache(fs);

    // Check if the directory is empty, if it enters the loop then it means it is not empty.
    if (!ext2_directory_is_empty(fs, cache, &inode)) {
        pr_err("Directory `%u` is not empty.\n", inode_index);
        ext2_dealloc_cache(cache);
        return -ENOTEMPTY;
    }

    // Get the group index of the parent
    uint32_t group_index = ext2_inode_index_to_group_index(fs, parent_index);

    // Decrease the number of directories in the group
    if (fs->block_groups[group_index].used_dirs_count == 0) {
        pr_err("Directory count underflow in block group `%u`.\n", group_index);
        ext2_dealloc_cache(cache);
        return -EINVAL;
    }
    fs->block_groups[group_index].used_dirs_count--;

    // Reduce the number of links to the parent directory.
    parent.links_count--;
    if (ext2_write_inode(fs, &parent, parent_index) < 0) {
        pr_err("Failed to update the inode of `%d`.\n", parent_index);
        ext2_dealloc_cache(cache);
        return -1;
    }

    // Reduce the number of links to the directory.
    inode.links_count = 0;

    // Update the delete time.
    inode.dtime = sys_time(NULL);

    if (ext2_write_inode(fs, &inode, inode_index) < 0) {
        pr_err("Failed to update the inode of `%d`.\n", inode_index);
        ext2_dealloc_cache(cache);
        return -1;
    }

    // Free the inode.
    if (ext2_free_inode(fs, &inode, inode_index) < 0) {
        pr_err("Failed to update inode `%u`.\n", inode_index);
        ext2_dealloc_cache(cache);
        return -1;
    }

    // Read the block where the direntry resides.
    if (ext2_read_inode_block(fs, &parent, block_index, cache) == -1) {
        pr_err("Failed to read block `%u` for parent inode `%u`.\n", block_index, parent_index);
        ext2_dealloc_cache(cache);
        return -1;
    }

    // Get a pointer to the direntry.
    ext2_dirent_t *dirent = (ext2_dirent_t *)((uintptr_t)cache + block_offset);
    if (!dirent) {
        pr_err("Found a NULL directory entry at offset `%u` in block `%u`.\n", block_offset, block_index);
        ext2_dealloc_cache(cache);
        return -1;
    }

    // Set the inode to zero.
    dirent->inode = 0;

    // Write back the parent directory block.
    if (!ext2_write_inode_block(fs, &parent, parent_index, block_index, cache)) {
        pr_err("Failed to write block `%u` for parent inode `%u`.\n", block_index, parent_index);
        ext2_dealloc_cache(cache);
        return -1;
    }

    // Free the cache.
    ext2_dealloc_cache(cache);
    return 0;
}

/// @brief Finds the entry with the given `name` inside the `directory`.
/// @param fs a pointer to the filesystem.
/// @param ino the inodex of the directory entry.
/// @param name the name of the entry we are looking for.
/// @param search the output variable where we save the info about the entry.
/// @return 0 on success, -errno on failure.
int ext2_find_direntry(ext2_filesystem_t *fs, ino_t ino, const char *name, ext2_direntry_search_t *search)
{
    if (fs == NULL) {
        pr_err("You provided a NULL filesystem.\n");
        return -1;
    }
    if (name == NULL) {
        pr_err("You provided a NULL name.\n");
        return -1;
    }
    if (search == NULL) {
        pr_err("You provided a NULL search.\n");
        return -1;
    }
    // Get the inode associated with the file.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, ino) == -1) {
        pr_err("Failed to read the inode (%d).\n", ino);
        return -1;
    }
    // Check that the parent is a directory.
    if (!bitmask_check(inode.mode, S_IFDIR)) {
        pr_err(
            "The parent inode is not a directory (ino: %d, mode: %d, name: "
            "%s).\n",
            ino, inode.mode, name);
        return -1;
    }

    // Check that we are allowed to reach through the directory
    if (!ext2_valid_x_permission(scheduler_get_current_process(), &inode)) {
        pr_err(
            "The parent inode has no x permission (ino: %d, mode: %d, name: "
            "%s).\n",
            ino, inode.mode, name);
        return -EPERM;
    }

    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);

    // Prepare iterator.
    ext2_direntry_iterator_t it = ext2_direntry_iterator_begin(fs, cache, &inode);
    for (; ext2_direntry_iterator_valid(&it); ext2_direntry_iterator_next(&it)) {
        // Skip unused inode.
        if (it.direntry->inode == 0) {
            continue;
        }
        // Chehck the name.
        if (!strncmp(it.direntry->name, ".", 1) && !strncmp(name, "/", 1)) {
            break;
        }
        // Check if the entry has the same name.
        if (strlen(name) == it.direntry->name_len) {
            if (!strncmp(it.direntry->name, name, it.direntry->name_len)) {
                break;
            }
        }
    }
    // Copy the inode of the parent, even if we did not find the entry.
    search->parent_inode = ino;
    // Check if we have found the entry.
    if (it.direntry == NULL) {
        goto free_cache_return_error;
    }
    // Copy the direntry.
    search->direntry.inode     = it.direntry->inode;
    search->direntry.rec_len   = it.direntry->rec_len;
    search->direntry.name_len  = it.direntry->name_len;
    search->direntry.file_type = it.direntry->file_type;
    strncpy(search->direntry.name, it.direntry->name, it.direntry->name_len);
    search->direntry.name[it.direntry->name_len] = 0;
    // Copy the index of the block containing the direntry.
    search->block_index                          = it.block_index;
    // Copy the offset of the direntry inside the block.
    search->block_offset                         = it.block_offset;
    // Free the cache.
    ext2_dealloc_cache(cache);
    return 0;
free_cache_return_error:
    // Free the cache.
    ext2_dealloc_cache(cache);
    return -1;
}
