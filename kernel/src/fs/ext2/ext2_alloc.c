/// @file ext2_alloc.c
/// @brief EXT2 allocation and release of inodes and blocks.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[EXT2  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "ext2_internal.h"

/// @brief Searches for a free inode inside the group data loaded inside the cache.
/// @param fs the ext2 filesystem structure.
/// @param cache the cache from which we read the bgdt data.
/// @param group_offset the output variable where we store the linear indes to the free inode.
/// @param skip_reserved should we skip reserved inodes.
/// @return 1 if we found a free inode, 0 otherwise.
static inline int
ext2_find_free_inode_in_group(ext2_filesystem_t *fs, uint8_t *cache, uint32_t *group_offset, int skip_reserved)
{
    for ((*group_offset) = 0; (*group_offset) < fs->superblock.inodes_per_group; ++(*group_offset)) {
        // If we need to skip the reserved inodes, we skip the round if the
        // index is that of a reserved inode (superblock.first_ino).
        if (skip_reserved && ((*group_offset) < fs->superblock.first_ino)) {
            continue;
        }
        // Check if the entry is free.
        if (!ext2_bitmap_check(cache, *group_offset)) {
            return 1;
        }
    }
    return 0;
}

/// @brief Searches for a free inode inside the Block Group Descriptor Table (BGDT).
/// @param fs the ext2 filesystem structure.
/// @param cache the cache from which we read the bgdt data.
/// @param group_index the output variable where we store the group index.
/// @param group_offset the output variable where we store the linear indes to the free inode.
/// @param preferred_group we accept a preferred group, but only if available.
/// @return 1 if we found a free inode, 0 otherwise.
static inline int ext2_find_free_inode(
    ext2_filesystem_t *fs,
    uint8_t *cache,
    uint32_t *group_index,
    uint32_t *group_offset,
    uint32_t preferred_group)
{
    // If we received a preference, try to find a free inode in that specific group.
    if (preferred_group != 0) {
        // Set the group index to the preferred group.
        (*group_index) = preferred_group;
        // Find the first free inode. We need to ask to skip reserved inodes,
        // only if we are in group 0.
        if (ext2_find_free_inode_in_group(fs, cache, group_offset, (*group_index) == 0)) {
            return 1;
        }
    }
    // Get the group and bit index of the first free block.
    for ((*group_index) = 0; (*group_index) < fs->block_groups_count; ++(*group_index)) {
        // Check if there are free inodes in this block group.
        if (fs->block_groups[(*group_index)].free_inodes_count > 0) {
            // Read the block bitmap.
            if (ext2_read_block(fs, fs->block_groups[(*group_index)].inode_bitmap, cache) < 0) {
                pr_err("Failed to read the inode bitmap for group `%d`.\n", (*group_index));
                return 0;
            }
            // Find the first free inode. We need to ask to skip reserved
            // inodes, only if we are in group 0.
            if (ext2_find_free_inode_in_group(fs, cache, group_offset, (*group_index) == 0)) {
                return 1;
            }
        }
    }
    return 0;
}

/// @brief Searches for a free block inside the group data loaded inside the cache.
/// @param fs the ext2 filesystem structure.
/// @param cache the cache from which we read the bgdt data.
/// @param block_offset the output variable where we store the linear indes to the free block.
/// @return 1 if we found a free block, 0 otherwise.
static inline int ext2_find_free_block_in_group(ext2_filesystem_t *fs, uint8_t *cache, uint32_t *block_offset)
{
    for ((*block_offset) = 0; (*block_offset) < fs->superblock.blocks_per_group; ++(*block_offset)) {
        // Check if the entry is free.
        if (!ext2_bitmap_check(cache, *block_offset)) {
            return 1;
        }
    }
    return 0;
}

/// @brief Searches for a free block.
/// @param fs the ext2 filesystem structure.
/// @param cache the cache from which we read the bgdt data.
/// @param group_index the output variable where we store the group index.
/// @param block_offset the output variable where we store the linear indes to the free block.
/// @return 1 if we found a free block, 0 otherwise.
static inline int
ext2_find_free_block(ext2_filesystem_t *fs, uint8_t *cache, uint32_t *group_index, uint32_t *block_offset)
{
    // Get the group and bit index of the first free block.
    for ((*group_index) = 0; (*group_index) < fs->block_groups_count; ++(*group_index)) {
        // Check if there are free blocks in this block group.
        if (fs->block_groups[(*group_index)].free_blocks_count > 0) {
            // Read the block bitmap.
            if (ext2_read_block(fs, fs->block_groups[(*group_index)].block_bitmap, cache) < 0) {
                pr_err("Failed to read the block bitmap for group `%d`.\n", (*group_index));
                return 0;
            }
            // Find the first free block.
            if (ext2_find_free_block_in_group(fs, cache, block_offset)) {
                return 1;
            }
        }
    }
    return 0;
}

/// @brief Allocate a new inode.
/// @param fs the filesystem.
/// @param preferred_group the preferred group.
/// @return index of the inode.
/// @details
/// Here are the rules used to allocate new inodes:
///  - the inode for a new file is allocated in the same group of the inode of
///    its parent directory.
///  - inodes are allocated equally between groups.
int ext2_allocate_inode(ext2_filesystem_t *fs, unsigned preferred_group)
{
    uint32_t group_index  = 0;
    uint32_t group_offset = 0;
    uint32_t inode_index  = 0;
    // Lock the filesystem.
    spinlock_lock(&fs->spinlock);
    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);
    // Search for a free inode.
    if (!ext2_find_free_inode(fs, cache, &group_index, &group_offset, preferred_group)) {
        pr_err("Failed to find a free inode.\n");
        // Unlock the filesystem.
        spinlock_unlock(&fs->spinlock);
        // Free the cache.
        ext2_dealloc_cache(cache);
        return 0;
    }
    // Compute the inode index.
    inode_index = (group_index * fs->superblock.inodes_per_group) + group_offset + 1U;
    // Log the allocation of the inode.
    pr_debug(
        "ext2_allocate_inode(inode: %4u, group_index: %4u, group_offset: %4u\n", inode_index, group_index,
        group_offset);
    // Set the inode as occupied.
    ext2_bitmap_set(cache, group_offset);
    // Write back the inode bitmap.
    if (ext2_write_block(fs, fs->block_groups[group_index].inode_bitmap, cache) < 0) {
        pr_err("We failed to write back the block_bitmap.\n");
    }
    // Free the cache.
    ext2_dealloc_cache(cache);
    // Reduce the number of free inodes.
    fs->block_groups[group_index].free_inodes_count--;
    // Reduce the number of inodes inside the superblock.
    fs->superblock.free_inodes_count--;
    // Update only the affected BGDT block (more efficient than writing all blocks).
    if (ext2_write_bgdt_for_group(fs, group_index) < 0) {
        pr_warning("Failed to write BGDT for group %u.\n", group_index);
    }
    // Update the superblock.
    if (ext2_write_superblock(fs) < 0) {
        pr_warning("Failed to write superblock.\n");
    }
    // Unlock the filesystem.
    spinlock_unlock(&fs->spinlock);
    // Return the inode.
    return inode_index;
}

/// @brief Allocates a new block.
/// @param fs the filesystem.
/// @return 0 on failure, or the index of the new block on success.
uint32_t ext2_allocate_block(ext2_filesystem_t *fs)
{
    uint32_t group_index  = 0;
    uint32_t group_offset = 0;
    uint32_t block_index  = 0;
    // Lock the filesystem.
    spinlock_lock(&fs->spinlock);
    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);
    // Search for a free block.
    if (!ext2_find_free_block(fs, cache, &group_index, &group_offset)) {
        pr_err("Failed to find a free block.\n");
        // Unlock the filesystem.
        spinlock_unlock(&fs->spinlock);
        // Free the cache.
        ext2_dealloc_cache(cache);
        return 0;
    }
    // Compute the block index.
    block_index = (group_index * fs->superblock.blocks_per_group) + group_offset;
    // Log the allocation of the inode.
    pr_debug(
        "ext2_allocate_block(block: %4u, group_index: %4u, group_offset: "
        "%4u)\n",
        block_index, group_index, group_offset);
    // Set the block as occupied.
    ext2_bitmap_set(cache, group_offset);
    // Update the bitmap.
    if (ext2_write_block(fs, fs->block_groups[group_index].block_bitmap, cache) < 0) {
        pr_err("We failed to write back the block_bitmap.\n");
    }
    // Decrease the number of free blocks inside the BGDT entry.
    fs->block_groups[group_index].free_blocks_count--;
    // Decrease the number of free blocks inside the superblock.
    fs->superblock.free_blocks_count--;
    // Update only the affected BGDT block (more efficient than writing all blocks).
    if (ext2_write_bgdt_for_group(fs, group_index) < 0) {
        pr_warning("Failed to write BGDT for group %u.\n", group_index);
    }
    // Update the superblock.
    if (ext2_write_superblock(fs) < 0) {
        pr_warning("Failed to write superblock.\n");
    }
    // Empty out the new block content.
    memset(cache, 0, fs->block_size);
    // Write the empty content of the new block.
    if (ext2_write_block(fs, block_index, cache) < 0) {
        pr_err("We failed to clean the content of the newly allocated block.\n");
    }
    // Free the cache.
    ext2_dealloc_cache(cache);
    // Unlock the spinlock.
    spinlock_unlock(&fs->spinlock);
    return block_index;
}

/// @brief Frees a block.
/// @param fs the filesystem.
/// @param block_index the index of the block we are freeing.
/// @return 0 on success, -1 when the block could not be released.
/// @details A block that cannot be freed is a leak. A group whose bitmap is
///          wrongly rewritten is corruption, and the allocator will hand out
///          blocks that are in use. Leaking is by far the lesser harm, so
///          nothing is written unless the bitmap was read first (#342).
int ext2_free_block(ext2_filesystem_t *fs, uint32_t block_index)
{
    uint32_t group_index  = ext2_block_index_to_group_index(fs, block_index);
    uint32_t group_offset = ext2_block_index_to_group_offset(fs, block_index);
    uint32_t block_bitmap = fs->block_groups[group_index].block_bitmap;

    // Log the allocation of the inode.
    pr_debug(
        "ext2_free_block(block: %4u, group_index: %4u, group_offset: %4u)\n", block_index, group_index, group_offset);

    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);
    if (cache == NULL) {
        pr_err("Failed to allocate the cache to free block %u.\n", block_index);
        return -1;
    }
    // Read the bitmap. `ext2_alloc_cache` zeroes what it returns, so a failed
    // read leaves an all-zero bitmap rather than a stale one: writing that
    // back would mark every block in the group free.
    if (ext2_read_block(fs, block_bitmap, cache) < 0) {
        pr_err("Failed to read the block bitmap %u; block %u stays allocated.\n", block_bitmap, block_index);
        ext2_dealloc_cache(cache);
        return -1;
    }
    // Set it as free.
    ext2_bitmap_clear(cache, group_offset);
    // Write back the block bitmap.
    if (ext2_write_block(fs, block_bitmap, cache) < 0) {
        pr_err("We failed to write back the block_bitmap.\n");
        ext2_dealloc_cache(cache);
        return -1;
    }
    // Free the cache.
    ext2_dealloc_cache(cache);

    // Increase the number of free blocks inside the superblock.
    fs->superblock.free_blocks_count++;
    // Increase the number of free blocks inside the BGDT entry.
    fs->block_groups[group_index].free_blocks_count++;
    // Update only the affected BGDT block (more efficient than writing all blocks).
    if (ext2_write_bgdt_for_group(fs, group_index) < 0) {
        pr_warning("Failed to write BGDT for group %u.\n", group_index);
    }
    // Update the superblock.
    if (ext2_write_superblock(fs) < 0) {
        pr_warning("Failed to write superblock.\n");
    }
    return 0;
}

/// @brief Frees the blocks that hold the block pointers of an inode.
/// @param fs the filesystem.
/// @param inode the inode whose index blocks are freed.
/// @return 0 when every block was released, -1 when any was kept.
/// @details The data blocks of a file are reached through these, so they must
///          be freed after the data blocks, and they belong to the file just
///          as much: leaving them behind lost a block per indirection level on
///          every deletion (#302).
static int __ext2_free_index_blocks(ext2_filesystem_t *fs, ext2_inode_t *inode)
{
    uint32_t pointers = fs->pointers_per_block;
    uint8_t *outer    = ext2_alloc_cache(fs);
    uint8_t *inner    = ext2_alloc_cache(fs);
    if ((outer == NULL) || (inner == NULL)) {
        pr_err("Failed to allocate the cache to free the index blocks.\n");
        ext2_dealloc_cache(outer);
        ext2_dealloc_cache(inner);
        return -1;
    }
    // An index block is the only record of which blocks it points at, so
    // freeing one whose contents could not be read loses that record for good:
    // the blocks under it stay marked used with nothing naming them, and no
    // later pass can find them. Every level below therefore keeps the index
    // block when its read fails, and reports it. A leak that is still
    // described on disk can be recovered by a checker; one whose description
    // has been released cannot (#343).
    int failure = 0;

    // The single-indirect block holds pointers only.
    if (inode->data.blocks.indir_block != 0) {
        if (ext2_free_block(fs, inode->data.blocks.indir_block) < 0) {
            failure = -1;
        } else {
            inode->data.blocks.indir_block = 0;
        }
    }
    // The doubly-indirect block points at a level of index blocks.
    if (inode->data.blocks.doubly_indir_block != 0) {
        if (ext2_read_block(fs, inode->data.blocks.doubly_indir_block, outer) == -1) {
            pr_err(
                "Cannot read the doubly-indirect block %u: keeping it, so what it points at stays findable.\n",
                inode->data.blocks.doubly_indir_block);
            failure = -1;
        } else {
            for (uint32_t index = 0; index < pointers; ++index) {
                uint32_t block = ((uint32_t *)outer)[index];
                if ((block != 0) && (ext2_free_block(fs, block) < 0)) {
                    failure = -1;
                }
            }
            if (ext2_free_block(fs, inode->data.blocks.doubly_indir_block) < 0) {
                failure = -1;
            } else {
                inode->data.blocks.doubly_indir_block = 0;
            }
        }
    }
    // The trebly-indirect block points at two levels of index blocks.
    if (inode->data.blocks.trebly_indir_block != 0) {
        if (ext2_read_block(fs, inode->data.blocks.trebly_indir_block, outer) == -1) {
            pr_err(
                "Cannot read the trebly-indirect block %u: keeping it, so what it points at stays findable.\n",
                inode->data.blocks.trebly_indir_block);
            failure = -1;
        } else {
            for (uint32_t index = 0; index < pointers; ++index) {
                uint32_t middle = ((uint32_t *)outer)[index];
                if (middle == 0) {
                    continue;
                }
                if (ext2_read_block(fs, middle, inner) == -1) {
                    pr_err("Cannot read the index block %u: keeping it, so what it points at stays findable.\n", middle);
                    failure = -1;
                    continue;
                }
                for (uint32_t inner_index = 0; inner_index < pointers; ++inner_index) {
                    uint32_t block = ((uint32_t *)inner)[inner_index];
                    if ((block != 0) && (ext2_free_block(fs, block) < 0)) {
                        failure = -1;
                    }
                }
                if (ext2_free_block(fs, middle) < 0) {
                    failure = -1;
                }
            }
            // Only release the top of the tree once every level under it is
            // accounted for, for the same reason.
            if (failure == 0) {
                if (ext2_free_block(fs, inode->data.blocks.trebly_indir_block) < 0) {
                    failure = -1;
                } else {
                    inode->data.blocks.trebly_indir_block = 0;
                }
            }
        }
    }
    ext2_dealloc_cache(outer);
    ext2_dealloc_cache(inner);
    return failure;
}

/// @brief Frees every block that belongs to an inode, data and index alike.
/// @param fs a pointer to the filesystem.
/// @param inode the inode whose blocks are released.
/// @return 0 when every block was released, -1 when any was kept.
/// @details The block pointers and the sector count are cleared, so the inode
///          is left describing a file with no blocks at all. `size` is the
///          caller's: releasing the blocks of a file and declaring it empty
///          are two different things, and only truncation does both.
int ext2_free_inode_blocks(ext2_filesystem_t *fs, ext2_inode_t *inode)
{
    // How many blocks the size says the file holds. Sparse files legally
    // contain holes, so a null pointer inside that range is skipped rather
    // than treated as the end.
    uint32_t block_number = (inode->size / fs->block_size) + ((inode->size % fs->block_size) != 0);

    // Free the data blocks first: they are reached through the index blocks,
    // which the next step releases.
    int failure = 0;
    for (uint32_t block_index = 0; block_index < block_number; ++block_index) {
        // Get the real index.
        uint32_t real_index = ext2_get_real_block_index(fs, inode, block_index);
        if (real_index == 0) {
            continue;
        }
        if (ext2_free_block(fs, real_index) < 0) {
            failure = -1;
        }
    }

    // Free the blocks that held the pointers to those data blocks (#302).
    if (__ext2_free_index_blocks(fs, inode) < 0) {
        failure = -1;
    }

    // Nothing points anywhere any more, and the inode owns no sector. The
    // write path derives the blocks it still has to allocate from
    // `blocks_count`, so leaving it set would make it skip allocating the
    // blocks it just gave back.
    // Only claim the inode owns nothing when it really owns nothing. Clearing
    // the pointers after a block was kept would lose the record of it, which
    // is the mistake #343 is about one level down.
    if (failure == 0) {
        for (uint32_t index = 0; index < EXT2_DIRECT_BLOCKS; ++index) {
            inode->data.blocks.dir_blocks[index] = 0;
        }
        inode->blocks_count = 0;
    }
    return failure;
}

/// @brief Frees the given inode.
/// @param fs a pointer to the filesystem.
/// @param inode the inode we free.
/// @param inode_index its index.
/// @return 0 on success, otherwise it is a failure.
int ext2_free_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index)
{
    // Retrieve the group index.
    uint32_t group_index  = ext2_inode_index_to_group_index(fs, inode_index);
    // Get the index of the inode inside the group.
    uint32_t group_offset = ext2_inode_index_to_group_offset(fs, inode_index);
    // Get the block bitmap index.
    uint32_t inode_bitmap = fs->block_groups[group_index].inode_bitmap;

    // Log the allocation of the inode.
    pr_debug(
        "ext2_free_inode(group: %4u, inode_index: %4u, group_offset: %4u)\n", group_index, inode_index, group_offset);

    // Give back every block the inode owns, data and index alike. A block that
    // stays behind is a leak, and the inode is being freed either way, so this
    // is reported rather than aborted: stopping here would leave the inode
    // marked in use as well as the blocks.
    if (ext2_free_inode_blocks(fs, inode) < 0) {
        pr_err("Failed to release every block of inode %u; they stay allocated.\n", inode_index);
    }

    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);
    if (cache == NULL) {
        pr_err("Failed to allocate the cache to free inode %u.\n", inode_index);
        return -1;
    }
    // Read the bitmap. As in ext2_free_block: the cache comes back zeroed, so
    // writing it after a failed read would mark every inode in the group free
    // (#342). An inode that stays allocated is a leak; a group wrongly marked
    // free lets the allocator reuse inodes that are in use.
    if (ext2_read_block(fs, inode_bitmap, cache) < 0) {
        pr_err("Failed to read the inode bitmap %u; inode %u stays allocated.\n", inode_bitmap, inode_index);
        ext2_dealloc_cache(cache);
        return -1;
    }
    // Set it as free.
    ext2_bitmap_clear(cache, group_offset);
    // Write back the inode bitmap.
    if (ext2_write_block(fs, inode_bitmap, cache) < 0) {
        pr_err("We failed to write back the inode_bitmap.\n");
        ext2_dealloc_cache(cache);
        return -1;
    }
    // Free the cache.
    ext2_dealloc_cache(cache);

    // Increase the number of inodes inside the superblock.
    fs->superblock.free_inodes_count++;
    // Increase the number of free inodes.
    fs->block_groups[group_index].free_inodes_count++;
    // Update only the affected BGDT block (more efficient than writing all blocks).
    if (ext2_write_bgdt_for_group(fs, group_index) < 0) {
        pr_warning("Failed to write BGDT for group %u.\n", group_index);
    }
    // Update the superblock.
    if (ext2_write_superblock(fs) < 0) {
        pr_warning("Failed to write superblock.\n");
    }
    // Return the error code.
    return 0;
}
