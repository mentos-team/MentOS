/// @file ext2_io.c
/// @brief EXT2 transfers between the block device and the in-memory structures.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[EXT2  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "ext2_internal.h"

// Used before it is defined below.
static int ext2_write_bgdt(ext2_filesystem_t *fs);

/// @brief Allocate cache for EXT2 operations.
/// @param fs file system we are working with.
/// @return a pointer to the cache.
uint8_t *ext2_alloc_cache(ext2_filesystem_t *fs)
{
    // Validate input.
    if (!fs) {
        pr_err("Invalid input: filesystem pointer is NULL.");
        return NULL;
    }

    // Allocate the cache.
    uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    if (!cache) {
        // Log critical error if cache allocation fails.
        pr_crit("Failed to allocate cache for EXT2 operations.");
        return NULL;
    }

#ifdef ENABLE_EXT2_TRACE
    store_resource_info(ext2_resource_id, __RELATIVE_PATH__, 0, cache);
#endif

    // Clean the cache.
    memset(cache, 0, fs->block_size);

    return cache;
}

/// @brief Free the cache.
/// @param cache pointer to the cache.
void ext2_dealloc_cache(uint8_t *cache)
{
    // Validate the input.
    if (!cache) {
        pr_err("Invalid input: cache pointer is NULL or already freed.");
        return;
    }

#ifdef ENABLE_EXT2_TRACE
    clear_resource_info(cache);
#endif

    // Free the cache.
    kmem_cache_free(cache);
}

/// @brief Reads the superblock from the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return the amount of data we read, or negative value for an error.
int ext2_read_superblock(ext2_filesystem_t *fs)
{
    pr_debug("Read superblock for EXT2 filesystem (0x%x)\n", fs);
    return vfs_read(fs->block_device, &fs->superblock, 1024, sizeof(ext2_superblock_t));
}

/// @brief Writes the superblock on the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return the amount of data we wrote, or negative value for an error.
int ext2_write_superblock(ext2_filesystem_t *fs)
{
    pr_debug("Write superblock for EXT2 filesystem (0x%x)\n", fs);
    return vfs_write(fs->block_device, &fs->superblock, 1024, sizeof(ext2_superblock_t));
}

/// @brief Syncs the filesystem to disk by writing the superblock and all BGDT blocks.
/// @details This function ensures that the superblock and block group descriptor table
/// are persisted to disk. This should be called after batch operations like FHS initialization
/// or when an explicit sync is needed. Inode/block bitmaps and data blocks are already
/// written during allocation, so this is safe to defer.
/// @param fs the ext2 filesystem structure.
/// @return 0 on success, negative value on failure.
static int ext2_sync(ext2_filesystem_t *fs)
{
    pr_debug("ext2_sync(%p) - syncing superblock and BGDT to disk\n", fs);

    if (!fs) {
        pr_err("Invalid filesystem pointer for sync.\n");
        return -1;
    }

    // Write the entire BGDT (all affected blocks)
    if (ext2_write_bgdt(fs) < 0) {
        pr_warning("Failed to sync BGDT.\n");
        return -1;
    }

    // Write the superblock
    if (ext2_write_superblock(fs) < 0) {
        pr_warning("Failed to sync superblock.\n");
        return -1;
    }

    pr_debug("ext2_sync() completed successfully\n");
    return 0;
}

/// @brief Read a block from the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @param block_index the index of the block we want to read.
/// @param buffer the buffer where the content will be placed.
/// @return the amount of data we read, or negative value for an error.
int ext2_read_block(ext2_filesystem_t *fs, uint32_t block_index, uint8_t *buffer)
{
    if (block_index == 0) {
        pr_err("You are trying to read an invalid block index (%d).\n", block_index);
        return -1;
    }
    if (buffer == NULL) {
        pr_err("You are trying to read with a NULL buffer.\n");
        return -1;
    }
    uint64_t offset = (uint64_t)block_index * fs->block_size;
    return vfs_read(fs->block_device, buffer, offset, fs->block_size);
}

/// @brief Writes a block on the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @param block_index the index of the block we want to read.
/// @param buffer the buffer where the content will be placed.
/// @return the amount of data we wrote, or negative value for an error.
int ext2_write_block(ext2_filesystem_t *fs, uint32_t block_index, uint8_t *buffer)
{
    if (block_index == 0) {
        pr_err("You are trying to write on an invalid block index (%d).\n", block_index);
        return -1;
    }
    if (buffer == NULL) {
        pr_err("You are trying to write with a NULL buffer.\n");
        return -1;
    }
    return vfs_write(fs->block_device, buffer, block_index * fs->block_size, fs->block_size);
}

/// @brief Reads the Block Group Descriptor Table (BGDT) from the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return 0 on success, -1 on failure.
int ext2_read_bgdt(ext2_filesystem_t *fs)
{
    pr_debug("ext2_read_bgdt(%p)\n", fs);
    if (!fs->block_groups) {
        pr_err("The `block_groups` list is not initialized.\n");
        return -1;
    }
    for (uint32_t i = 0; i < fs->bgdt_length; ++i) {
        // A descriptor block that cannot be read leaves that slice of
        // `block_groups` holding whatever it held before, and the free counts
        // and bitmap locations for those groups would then be fiction. The
        // failure was discarded and the mount continued on it (#344).
        if (ext2_read_block(fs, fs->bgdt_start_block + i, (uint8_t *)((uintptr_t)fs->block_groups + (fs->block_size * i))) < 0) {
            pr_err("Failed to read BGDT block %u of %u.\n", i, fs->bgdt_length);
            return -1;
        }
    }
    return 0;
}

/// @brief Writes a specific block group descriptor block to the block device.
/// @details This is more efficient than writing the entire BGDT when only one
/// block group has been modified.
/// @param fs the ext2 filesystem structure.
/// @param group_index the index of the block group to write.
/// @return 0 on success, -1 on failure.
int ext2_write_bgdt_for_group(ext2_filesystem_t *fs, uint32_t group_index)
{
    if (!fs->block_groups) {
        pr_err("The `block_groups` list is not initialized.\n");
        return -1;
    }
    // Each block group descriptor is 32 bytes. Calculate which BGDT block contains this group.
    uint32_t descriptors_per_block = fs->block_size / sizeof(ext2_group_descriptor_t);
    uint32_t bgdt_block_index      = group_index / descriptors_per_block;

    if (bgdt_block_index >= fs->bgdt_length) {
        pr_err("Block group %u descriptor is out of BGDT range.\n", group_index);
        return -1;
    }

    pr_debug("ext2_write_bgdt_for_group(group: %u, bgdt_block: %u)\n", group_index, bgdt_block_index);
    // Write only the specific BGDT block containing this group's descriptor.
    if (ext2_write_block(fs, fs->bgdt_start_block + bgdt_block_index, (uint8_t *)((uintptr_t)fs->block_groups + (fs->block_size * bgdt_block_index))) < 0) {
        pr_err("Failed to write BGDT block %u for group %u.\n", bgdt_block_index, group_index);
        return -1;
    }
    return 0;
}

/// @brief Writes the Block Group Descriptor Table (BGDT) to the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return 0 on success, -1 on failure.
static int ext2_write_bgdt(ext2_filesystem_t *fs)
{
    pr_debug("ext2_write_bgdt(%p)\n", fs);
    if (!fs->block_groups) {
        pr_err("The `block_groups` list is not initialized.\n");
        return -1;
    }
    int failure = 0;
    for (uint32_t i = 0; i < fs->bgdt_length; ++i) {
        // Keep going: the descriptors that can be written should be, and the
        // caller still has to learn that some could not.
        if (ext2_write_block(fs, fs->bgdt_start_block + i, (uint8_t *)((uintptr_t)fs->block_groups + (fs->block_size * i))) < 0) {
            pr_err("Failed to write BGDT block %u of %u.\n", i, fs->bgdt_length);
            failure = -1;
        }
    }
    return failure;
}

/// @brief Reads an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @return 0 on success, -1 on failure.
int ext2_read_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index)
{
    uint32_t group_index;
    uint32_t block_index;
    uint32_t group_offset;
    if (inode_index == 0) {
        pr_err("You are trying to read an invalid inode index (%d).\n", inode_index);
        return -1;
    }
    // Retrieve the group index.
    group_index  = ext2_inode_index_to_group_index(fs, inode_index);
    // Get the index of the inode inside the group.
    group_offset = ext2_inode_index_to_group_offset(fs, inode_index);
    // Get the block offest.
    block_index  = ext2_inode_index_to_block_index(fs, inode_index);

    // Log the address to the inode.
    // pr_debug("Read inode   (inode_index: %4u, group_index: %4u, group_offset: %4u, block_index: %4u)\n",
    //          inode_index, group_index, group_offset, block_index);

    // Check for error.
    if (group_index > fs->block_groups_count) {
        pr_err("Invalid group index computed from inode index `%d`.\n", inode_index);
        return -1;
    }

    // Get the real inode offset inside the block.
    group_offset %= fs->inodes_per_block_count;
    // Allocate the cache.
    uint8_t *cache        = ext2_alloc_cache(fs);
    // Read the block containing the inode table.
    uint32_t actual_block = fs->block_groups[group_index].inode_table + block_index;
    if (ext2_read_block(fs, actual_block, cache) < 0) {
        pr_err("Failed to read inode table block (inode %u, block %u).\n", inode_index, actual_block);
        ext2_dealloc_cache(cache);
        return -1;
    }
    // Save the inode content (ensure we start from a clean buffer).
    memset(inode, 0, sizeof(ext2_inode_t));
    memcpy(
        inode, (ext2_inode_t *)((uintptr_t)cache + (group_offset * fs->superblock.inode_size)), sizeof(ext2_inode_t));
    // Free the cache.
    ext2_dealloc_cache(cache);
    return 0;
}

/// @brief Writes the inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @return 0 on success, -1 on failure.
int ext2_write_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index)
{
    uint32_t group_index;
    uint32_t block_index;
    uint32_t group_offset;
    if (inode_index == 0) {
        pr_err("You are trying to read an invalid inode index (%d).\n", inode_index);
        return -1;
    }
    // Retrieve the group index.
    group_index  = ext2_inode_index_to_group_index(fs, inode_index);
    // Get the index of the inode inside the group.
    group_offset = ext2_inode_index_to_group_offset(fs, inode_index);
    // Get the block offest.
    block_index  = ext2_inode_index_to_block_index(fs, inode_index);

    // pr_debug("Write inode  (inode_index: %4u, group_index: %4u, group_offset: %4u, block_index: %4u)\n",
    //          inode_index, group_index, group_offset, block_index);

    // Check for error.
    if (group_index > fs->block_groups_count) {
        pr_err("Invalid group index computed from inode index `%d`.\n", inode_index);
        return -1;
    }

    // Get the real inode offset inside the block.
    group_offset %= fs->inodes_per_block_count;
    uint32_t table_block = fs->block_groups[group_index].inode_table + block_index;

    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);
    if (cache == NULL) {
        pr_err("Failed to allocate the cache to write inode %u.\n", inode_index);
        return -1;
    }
    // Read the block containing the inode table. This is a read-modify-write:
    // one inode is patched into a block that holds many, so the rest of the
    // block has to be the block. `ext2_alloc_cache` zeroes what it returns, so
    // a failed read here does not leave stale contents, it leaves *zeroes* —
    // and writing those back destroys every other inode in the block, which
    // with 4 KiB blocks and 128-byte inodes is 31 of them. Nothing may be
    // written when the read fails (#344).
    if (ext2_read_block(fs, table_block, cache) < 0) {
        pr_err("Failed to read inode table block %u while writing inode %u.\n", table_block, inode_index);
        ext2_dealloc_cache(cache);
        return -1;
    }
    // Write the inode.
    memcpy(
        (ext2_inode_t *)((uintptr_t)cache + (group_offset * fs->superblock.inode_size)), inode, sizeof(ext2_inode_t));
    // Write back the block. The result is the function's result: this used to
    // return 0 unconditionally, so an inode that never reached the disk was
    // reported as written.
    int ret = 0;
    if (ext2_write_block(fs, table_block, cache) < 0) {
        pr_err("Failed to write inode table block %u for inode %u.\n", table_block, inode_index);
        ret = -1;
    }
    // Free the cache.
    ext2_dealloc_cache(cache);
    return ret;
}
