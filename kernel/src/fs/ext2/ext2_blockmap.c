/// @file ext2_blockmap.c
/// @brief EXT2 mapping from a file-relative block to a block on the device.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[EXT2  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "ext2_internal.h"

/// @brief Allocates a new block for storing block indices, for an inode.
/// @param fs the filesystem.
/// @param current_index the current index, or if 0, where we store the new one.
/// @return 0 on success, -1 on failure.
static int __ext2_allocate_indexing_block_for_inode(ext2_filesystem_t *fs, uint32_t *current_index)
{
    if (!(*current_index)) {
        // Allocate a new block.
        uint32_t block_index = ext2_allocate_block(fs);
        if (block_index == 0) {
            pr_err("We failed to allocate a new block for inode block "
                   "indexing.\n");
            return -1;
        }
        // Update the index.
        *current_index = block_index;
    }
    return 0;
}

/// @brief Allocates a new block for storing block indices, for a block containing block indices.
/// @param fs the filesystem.
/// @param indexing_block the index of block that contains the indices.
/// @param cache the cache were we load the block content.
/// @param index the index inside the list of indices.
/// @return 0 on success, -1 on failure.
static int
__ext2_read_and_allocate_indexing_block(ext2_filesystem_t *fs, uint32_t indexing_block, uint8_t *cache, uint32_t index)
{
    // Read the doubly-indirect block (which contains pointers to indirect blocks).
    ext2_read_block(fs, indexing_block, cache);
    // Check if we need to allocate a new block.
    if (!((uint32_t *)cache)[index]) {
        // Allocate a new block.
        uint32_t block_index = ext2_allocate_block(fs);
        if (block_index == 0) {
            pr_err("We failed to allocate a new block for inode block "
                   "indexing.\n");
            return -1;
        }
        // Update the index.
        ((uint32_t *)cache)[index] = block_index;
        // Write the indexing block.
        if (ext2_write_block(fs, indexing_block, cache) < 0) {
            pr_err("We failed to write back the indexing block, after "
                   "generating a new block for inode block indexing.\n");
            return -1;
        }
    }
    return 0;
}

/// @brief Sets the real block index based on the block index inside an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index thwe inode index.
/// @param block_index the block index inside the inode.
/// @param real_index the real block number.
/// @return 0 on success, a negative value on failure.
static int ext2_set_real_block_index(
    ext2_filesystem_t *fs,
    ext2_inode_t *inode,
    uint32_t inode_index,
    uint32_t block_index,
    uint32_t real_index)
{
    // Get the number of pointers per block.
    unsigned int p = fs->pointers_per_block;
    // Help compute the indices.
    int a;
    int b;
    int c;
    int d;
    int e;
    int f;
    int g;
    // We save intermediate indices here.
    uint32_t index_save;
    // Result of the operation.
    int ret = 0;

    // Are we setting a DIRECT block pointer. The comparison has to match the
    // one in ext2_get_real_block_index: with `<= 0` the first index of each
    // region was stored one element past the end of the region before it,
    // where the reader never looks (#301).
    a = ((int)block_index) - EXT2_DIRECT_BLOCKS;
    if (a < 0) {
        inode->data.blocks.dir_blocks[block_index] = real_index;
    } else {
        // Allocate the cache.
        uint8_t *cache = ext2_alloc_cache(fs);
        // Are we setting an INDIRECT block pointer.
        b              = a - p;
        if (b < 0) {
            // Check that the indirect block points to a valid block.
            if (__ext2_allocate_indexing_block_for_inode(fs, &inode->data.blocks.indir_block)) {
                ret = -1;
                goto early_exit;
            }
            // Read the indirect block (which contains pointers to the next set of blocks).
            ext2_read_block(fs, inode->data.blocks.indir_block, cache);
            // Write the index inside the final block.
            ((uint32_t *)cache)[a] = real_index;
            // Write back the indirect block.
            ext2_write_block(fs, inode->data.blocks.indir_block, cache);

        } else {
            // Are we setting a DOUBLY-INDIRECT block.
            c = b - p * p;
            if (c < 0) {
                c = b / p;
                d = b - c * p;
                // Check that the indirect block points to a valid block.
                if (__ext2_allocate_indexing_block_for_inode(fs, &inode->data.blocks.doubly_indir_block)) {
                    ret = -1;
                    goto early_exit;
                }
                // Read the doubly-indirect block (which contains pointers to indirect blocks).
                if (__ext2_read_and_allocate_indexing_block(fs, inode->data.blocks.doubly_indir_block, cache, c)) {
                    ret = -1;
                    goto early_exit;
                }
                // Save the index.
                index_save = ((uint32_t *)cache)[c];
                // Compute the index inside the indirect block.
                ext2_read_block(fs, index_save, cache);
                // Write the index inside the final block.
                ((uint32_t *)cache)[d] = real_index;
                // Write back the indirect block.
                ext2_write_block(fs, index_save, cache);

            } else {
                d = c - p * p * p;
                if (d < 0) {
                    e = c / (p * p);
                    f = (c - e * p * p) / p;
                    g = (c - e * p * p - f * p);

                    // Check that the indirect block points to a valid block.
                    if (__ext2_allocate_indexing_block_for_inode(fs, &inode->data.blocks.trebly_indir_block)) {
                        ret = -1;
                        goto early_exit;
                    }
                    // Read the doubly-indirect block (which contains pointers to indirect blocks).
                    if (__ext2_read_and_allocate_indexing_block(fs, inode->data.blocks.trebly_indir_block, cache, e)) {
                        ret = -1;
                        goto early_exit;
                    }
                    // Save the index.
                    index_save = ((uint32_t *)cache)[e];
                    // Read the doubly-indirect block (which contains pointers to indirect blocks).
                    if (__ext2_read_and_allocate_indexing_block(fs, index_save, cache, f)) {
                        ret = -1;
                        goto early_exit;
                    }
                    // Save the index.
                    index_save = ((uint32_t *)cache)[f];
                    // Read the indirect block (which contains pointers to the next set of blocks).
                    ext2_read_block(fs, index_save, cache);
                    // Write the index inside the final block.
                    ((uint32_t *)cache)[g] = real_index;
                    // Write back the indirect block.
                    ext2_write_block(fs, index_save, cache);

                } else {
                    pr_err(
                        "We failed to write the real block number of the block "
                        "with index `%d`\n",
                        block_index);
                    ret = -1;
                }
            }
        }
    early_exit:
        // Free the cache.
        ext2_dealloc_cache(cache);
    }
    return ret;
}

/// @brief Returns the real block index starting from a block index inside an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param block_index the block index inside the inode.
/// @return the real block number.
uint32_t ext2_get_real_block_index(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t block_index)
{
    // Get the number of pointers per block.
    unsigned int p = fs->pointers_per_block;
    // Help compute the indices.
    int a;
    int b;
    int c;
    int d;
    int e;
    int f;
    int g;
    // The real index.
    uint32_t real_index = 0;

    // Check if the index is among the DIRECT blocks.
    a = block_index - EXT2_DIRECT_BLOCKS;
    if (a < 0) {
        real_index = inode->data.blocks.dir_blocks[block_index];
        pr_debug("ext2_get_real_block_index: direct block %u -> real_index %u\n", block_index, real_index);
    } else {
        // Allocate the cache.
        uint8_t *cache = ext2_alloc_cache(fs);
        // Check if the index is among the INDIRECT blocks.
        b              = a - p;
        if (b < 0) {
            // Read the indirect block (which contains pointers to the next set of blocks).
            if (inode->data.blocks.indir_block == 0) {
                pr_warning("ext2_get_real_block_index: indirect block not allocated (block_index=%u)\n", block_index);
                real_index = 0;
            } else {
                ext2_read_block(fs, inode->data.blocks.indir_block, cache);
                // Compute the index inside the final block.
                real_index = ((uint32_t *)cache)[a];
                pr_debug("ext2_get_real_block_index: indirect block %u (via block %u) -> real_index %u\n", block_index, inode->data.blocks.indir_block, real_index);
            }

        } else {
            // Check if the index is among the DOUBLY-INDIRECT blocks.
            c = b - p * p;
            if (c < 0) {
                // Compute the indirect indices.
                c = b / p;
                d = b - c * p;
                // Read the doubly-indirect block (which contains pointers to indirect blocks).
                if (inode->data.blocks.doubly_indir_block == 0) {
                    pr_warning("ext2_get_real_block_index: doubly-indirect block not allocated (block_index=%u)\n", block_index);
                    real_index = 0;
                } else {
                    ext2_read_block(fs, inode->data.blocks.doubly_indir_block, cache);
                    // Compute the index inside the indirect block.
                    uint32_t indir_blk = ((uint32_t *)cache)[c];
                    if (indir_blk == 0) {
                        pr_warning("ext2_get_real_block_index: indirect block pointer is 0 in doubly-indirect (c=%u)\n", c);
                        real_index = 0;
                    } else {
                        ext2_read_block(fs, indir_blk, cache);
                        // Compute the index inside the final block.
                        real_index = ((uint32_t *)cache)[d];
                        pr_debug("ext2_get_real_block_index: doubly-indirect block %u -> real_index %u\n", block_index, real_index);
                    }
                }

            } else {
                // Check if the index is among the TREBLY-INDIRECT blocks.
                d = c - p * p * p;
                if (d < 0) {
                    e = c / (p * p);
                    f = (c - e * p * p) / p;
                    g = (c - e * p * p - f * p);
                    // Read the trebly-indirect block (which contains pointers to doubly-indirect blocks).
                    if (inode->data.blocks.trebly_indir_block == 0) {
                        pr_warning("ext2_get_real_block_index: trebly-indirect block not allocated (block_index=%u)\n", block_index);
                        real_index = 0;
                    } else {
                        ext2_read_block(fs, inode->data.blocks.trebly_indir_block, cache);
                        // Read the doubly-indirect block (which contains pointers to indirect blocks).
                        uint32_t dblind_blk = ((uint32_t *)cache)[e];
                        if (dblind_blk == 0) {
                            pr_warning("ext2_get_real_block_index: doubly-indirect pointer is 0 in trebly-indirect (e=%u)\n", e);
                            real_index = 0;
                        } else {
                            ext2_read_block(fs, dblind_blk, cache);
                            uint32_t indir_blk = ((uint32_t *)cache)[f];
                            if (indir_blk == 0) {
                                pr_warning("ext2_get_real_block_index: indirect pointer is 0 in trebly-indirect (f=%u)\n", f);
                                real_index = 0;
                            } else {
                                // Read the indirect block (which contains pointers to the next set of blocks).
                                ext2_read_block(fs, indir_blk, cache);
                                // Compute the index inside the final block.
                                real_index = ((uint32_t *)cache)[g];
                                pr_debug("ext2_get_real_block_index: trebly-indirect block %u -> real_index %u\n", block_index, real_index);
                            }
                        }
                    }

                } else {
                    pr_err(
                        "We failed to retrieve the real block number of the "
                        "block with index `%d`\n",
                        block_index);
                }
            }
        }
        // Free the cache.
        ext2_dealloc_cache(cache);
    }
    return real_index;
}

/// @brief Allocate a new block for an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @param block_index The index of the block within the inode.
/// @return 0 on success, -1 on failure.
int ext2_allocate_inode_block(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index, uint32_t block_index)
{
    // Allocate the block.
    int real_index = ext2_allocate_block(fs);
    if (real_index == -1) {
        return -1;
    }
    // Associate the real index and the index inside the inode.
    if (ext2_set_real_block_index(fs, inode, inode_index, block_index, real_index) == -1) {
        return -1;
    }
    pr_debug("ext2_allocate_inode_block(inode: %4u, block: %4u, real: %4u)\n", inode_index, block_index, real_index);
    // Compute the new blocks count (in 512-byte sectors).
    uint32_t blocks_count = (block_index + 1) * fs->blocks_per_block_count;
    if (inode->blocks_count < blocks_count) {
        // Set the blocks count.
        uint32_t old_count  = inode->blocks_count;
        inode->blocks_count = blocks_count;
        pr_debug("Updated inode %d blocks_count: %u -> %u sectors (filesystem blocks: %u -> %u)\n", inode_index, old_count, inode->blocks_count, old_count / fs->blocks_per_block_count, inode->blocks_count / fs->blocks_per_block_count);
    }
    // Update the inode.
    if (ext2_write_inode(fs, inode, inode_index) == -1) {
        return -1;
    }
    return 0;
}

/// @brief Reads the real block starting from an inode and the block index inside the inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param block_index the index of the block within the inode.
/// @param buffer the buffer where to put the data.
/// @return the amount of data we read, or negative value for an error.
ssize_t ext2_read_inode_block(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t block_index, uint8_t *buffer)
{
    // Every block covered by the file size is a legitimate data block:
    // sparse files legally contain holes (a zero block pointer) inside
    // `inode->size`, which must read as a block of zeros. Rejecting holes
    // made any read touching them fail entirely (issue #192). Only blocks
    // beyond the file size are invalid. Note that `inode->blocks_count`
    // counts allocated blocks only, so it cannot be used as the upper
    // bound here.
    uint32_t max_blocks = inode->size / fs->block_size;
    if ((inode->size % fs->block_size) != 0) {
        max_blocks += 1;
    }
    // Check if block index is out of range.
    if (block_index >= max_blocks) {
        pr_err(
            "Invalid block index: %u >= %u blocks covered by the file size (file_size=%u)\n",
            block_index, max_blocks, inode->size);
        return -1;
    }

    // Get the real block index
    uint32_t real_index = ext2_get_real_block_index(fs, inode, block_index);

    // A resolved pointer of zero is a sparse hole: it has no block on disk
    // and must read as zeros. Block zero is never a valid data block in
    // ext2, and the metadata paths read their blocks through
    // ext2_read_block directly, so zero here always means hole (#192).
    if (real_index == 0) {
        memset(buffer, 0, fs->block_size);
        return fs->block_size;
    }

    // Log the resolved block index (debug level)
#ifdef EXT2_FULL_DEBUG
    pr_debug("ext2_read_inode_block(block: %4u, real: %4u)\n", block_index, real_index);
#endif

    // Read the block and return the result.
    return ext2_read_block(fs, real_index, buffer);
}

/// @brief Writes the real block starting from an inode and the block index inside the inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @param block_index the index of the block within the inode.
/// @param buffer the buffer where to put the data.
/// @return the amount of data we wrote, or negative value for an error.
ssize_t ext2_write_inode_block(
    ext2_filesystem_t *fs,
    ext2_inode_t *inode,
    uint32_t inode_index,
    uint32_t block_index,
    uint8_t *buffer)
{
    uint32_t total_blocks_needed;
    uint32_t allocated_blocks;
    uint32_t blocks_to_allocate;
    uint32_t real_index;

    // Calculate total blocks needed.
    total_blocks_needed = block_index + 1;

    // Calculate currently allocated blocks.
    allocated_blocks = inode->blocks_count / fs->blocks_per_block_count;

    // Determine additional blocks to allocate.
    blocks_to_allocate = 0;
    if (total_blocks_needed > allocated_blocks) {
        blocks_to_allocate = total_blocks_needed - allocated_blocks;
    }

    // Allocate necessary blocks
    while (blocks_to_allocate > 0) {
        if (ext2_allocate_inode_block(fs, inode, inode_index, allocated_blocks++) < 0) {
            pr_crit("Failed to allocate inode block\n");
            return -1;
        }
        blocks_to_allocate--;
    }

    // Get the real index.
    real_index = ext2_get_real_block_index(fs, inode, block_index);
    if (real_index == 0) {
        return -1;
    }

    // Log the address to the inode block.
#ifdef EXT2_FULL_DEBUG
    pr_debug("ext2_write_inode_block(block: %4u, inode: %4u, real: %4u)\n", block_index, inode_index, real_index);
#endif

    // Write the block.
    return ext2_write_block(fs, real_index, buffer);
}

/// @brief Reads the data from the given inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index the index of the inode.
/// @param offset the offset from which we start reading the data.
/// @param nbyte the number of bytes to read.
/// @param buffer the buffer containing the data.
/// @return the amount we read.
ssize_t ext2_read_inode_data(
    ext2_filesystem_t *fs,
    ext2_inode_t *inode,
    uint32_t inode_index,
    off_t offset,
    size_t nbyte,
    char *buffer)
{
    // A read at or beyond the end of the file reads zero bytes: standard
    // EOF semantics. Without this guard, an offset past `inode->size`
    // either hit the unallocated tail blocks of the loop below (returning
    // -1 at a legal EOF) or computed wrapping copy lengths in unsigned
    // arithmetic (issue #242).
    if ((uint64_t)offset >= (uint64_t)inode->size) {
        return 0;
    }
    // A zero-length read copies nothing.
    if (nbyte == 0) {
        return 0;
    }
    // Get the offset to the end of the portion we are reading.
    uint32_t end_offset  = (inode->size >= offset + nbyte) ? (offset + nbyte) : (inode->size);
    // Convert the offset/size to some starting/end iblock numbers. The end
    // block is the one containing the last byte to read (end_offset - 1):
    // when end_offset is an exact multiple of the block size, dividing
    // end_offset directly would point one block past the data and leave
    // end_size at zero, wrapping the copy length of the last block
    // (issue #242).
    uint32_t start_block = offset / fs->block_size;
    uint32_t end_block   = (end_offset - 1) / fs->block_size;
    // What's the offset into the start block.
    uint32_t start_off   = offset % fs->block_size;
    // How much bytes to read for the end block.
    uint32_t end_size    = end_offset - (end_block * fs->block_size);

#ifdef EXT2_FULL_DEBUG
    pr_debug("ext2_read_inode_data(inode: %4u, offset: %4u, nbyte: %4u)\n", inode_index, offset, nbyte);
#endif

    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);

    uint32_t curr_off = 0;
    uint32_t left;
    uint32_t right;
    uint32_t ret = end_offset - offset;
    for (uint32_t block_index = start_block; block_index <= end_block; ++block_index) {
        left = 0, right = fs->block_size - 1;
        // Read the real block.
        if (ext2_read_inode_block(fs, inode, block_index, cache) == -1) {
            pr_err("Failed to read the inode block %4u of inode %4u\n", block_index, inode_index);
            ext2_dealloc_cache(cache);
            return -1;
        }
        if (block_index == start_block) {
            left = start_off;
        }
        if (block_index == end_block) {
            right = end_size - 1;
        }
        // Copy the content back to the buffer.
        memcpy(buffer + curr_off, cache + left, (right - left + 1));
        // Move the offset.
        curr_off += (right - left + 1);
    }
    // Free the cache.
    ext2_dealloc_cache(cache);
    return ret;
}

/// @brief Writes the data on the given inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index the index of the inode.
/// @param offset the offset from which we start writing the data.
/// @param nbyte the number of bytes to write.
/// @param buffer the buffer containing the data.
/// @return the amount written.
ssize_t ext2_write_inode_data(
    ext2_filesystem_t *fs,
    ext2_inode_t *inode,
    uint32_t inode_index,
    off_t offset,
    size_t nbyte,
    char *buffer)
{
    // Prevent integer overflow: check if offset + nbyte would exceed UINT32_MAX
    if (offset > (UINT32_MAX - nbyte)) {
        pr_err("Integer overflow: offset + nbyte exceeds UINT32_MAX\n");
        return -1;
    }
    // Keep the size the file had, so that a write which cannot store every
    // block does not leave the inode claiming bytes that were never written.
    uint32_t original_size = inode->size;
    if ((offset + nbyte) > inode->size) {
        inode->size = offset + nbyte;
        if (ext2_write_inode(fs, inode, inode_index) == -1) {
            pr_err("Failed to write the inode `%d`\n", inode_index);
            return -1;
        }
    }
    // Get the offset to the end of the portion we are writing.
    uint32_t end_offset  = (inode->size >= offset + nbyte) ? (offset + nbyte) : (inode->size);
    // Convert the offset/size to some starting/end iblock numbers.
    uint32_t start_block = offset / fs->block_size;
    uint32_t end_block   = end_offset / fs->block_size;
    // What's the offset into the start block.
    uint32_t start_off   = offset % fs->block_size;
    // How much bytes to read for the end block.
    uint32_t end_size    = end_offset - (end_block * fs->block_size);

#ifdef EXT2_FULL_DEBUG
    pr_debug("ext2_write_inode_data(inode: %4u, offset: %4u, nbyte: %4u)\n", inode_index, offset, nbyte);
#endif

    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);

    uint32_t curr_off = 0;
    uint32_t left;
    uint32_t right;
    // Bytes actually stored, and the error to report if nothing could be.
    ssize_t written = 0;
    int failure     = 0;

    for (uint32_t block_index = start_block; block_index <= end_block; ++block_index) {
        // Recalculate on each iteration because ext2_write_inode_block may allocate new blocks.
        uint32_t allocated_fs_blocks = inode->blocks_count / fs->blocks_per_block_count;

        left = 0, right = fs->block_size - 1;

        if (block_index == start_block) {
            left = start_off;
        }
        if (block_index == end_block) {
            right = end_size - 1;
        }

        // Only read the block if it exists AND we're doing a partial write.
        // For full block writes or new blocks, we can skip the read.
        int is_partial_write = (left != 0) || (right != (fs->block_size - 1));
        int block_exists     = (block_index < allocated_fs_blocks);

        if (is_partial_write && block_exists) {
            // Read existing block for partial write (preserve unmodified portions).
            if (ext2_read_inode_block(fs, inode, block_index, cache) < 0) {
                pr_err("Failed to read block %u of inode %u for partial write\n", block_index, inode_index);
                failure = -EIO;
                break;
            }
        } else if (!block_exists) {
            // New block - zero it out to ensure clean state.
            memset(cache, 0, fs->block_size);
        }
        // else: full block overwrite of existing block - no need to read

        // Copy the content into the cache buffer.
        memcpy(cache + left, buffer + curr_off, (right - left + 1));
        // Move the offset.
        curr_off += (right - left + 1);
        // Write the block back. This returns -1 when the block could not be
        // allocated, and the number of bytes written otherwise: testing it as
        // a boolean let every failure through, so a full filesystem discarded
        // the data while reporting success (#303).
        if (ext2_write_inode_block(fs, inode, inode_index, block_index, cache) == -1) {
            pr_err("Failed to write the inode block %u of inode %u\n", block_index, inode_index);
            failure = -ENOSPC;
            break;
        }
        // The block is on the device: count what it stored.
        written += (right - left + 1);
    }
    // Free the cache.
    ext2_dealloc_cache(cache);

    if (failure) {
        // Shrink the size back to what is actually stored, keeping whatever
        // the file held before this call. A call that stored nothing leaves
        // the file exactly as it was, offset included.
        uint32_t stored_end = (written > 0) ? ((uint32_t)offset + (uint32_t)written) : original_size;
        uint32_t real_size  = (stored_end > original_size) ? stored_end : original_size;
        if (inode->size != real_size) {
            inode->size = real_size;
            if (ext2_write_inode(fs, inode, inode_index) == -1) {
                pr_err("Failed to restore the size of inode `%d`\n", inode_index);
            }
        }
        // A write that stored nothing is an error, one that stored part of
        // the data reports how much, as write(2) requires.
        if (written == 0) {
            return failure;
        }
    }
    return written;
}

/// @brief Cleans the inode content.
/// @param fs a pointer to the filesystem.
/// @param inode the inode.
/// @param inode_index the inode index.
/// @return 0 on success, 1 on failure.
int ext2_clean_inode_content(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index)
{
    pr_debug("ext2_clean_inode_content(%p, %p, %u)\n", fs, inode, inode_index);
    // Check the type of operation.
    if ((inode->mode & S_IFREG) != S_IFREG) {
        pr_alert("Trying to clean the content of a non-regular file.\n");
        return 1;
    }
    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);
    if (cache == NULL) {
        pr_err("Failed to allocate the cache to clean inode %u\n", inode_index);
        return -1;
    }
    // Get the cache size.
    size_t cache_size = fs->block_size;
    //
    int ret           = 0;
    for (ssize_t offset = 0, to_write; offset < inode->size;) {
        pr_debug(
            "ext2_clean_inode_content(%p, %p, %u): offset = %6u of size = %u\n", fs, inode, inode_index, offset,
            inode->size);
        // How many bytes are left to clean. The cache holds a single block, so
        // that is the most one call can store: asking for more made the driver
        // copy whatever followed the cache into the file, and the last request
        // reached past the end of the file and extended it (#315).
        size_t remaining = (size_t)(inode->size - (uint32_t)offset);
        to_write         = (ssize_t)min(cache_size, remaining);
        pr_debug("ext2_clean_inode_content(%p, %p, %u): to_write = %6u\n", fs, inode, inode_index, to_write);
        // Override the content.
        ssize_t written = ext2_write_inode_data(fs, inode, inode_index, offset, to_write, (char *)cache);
        pr_debug("ext2_clean_inode_content(%p, %p, %u): written = %6u\n", fs, inode, inode_index, written);
        if (written < 0) {
            pr_err("Failed to clean content of inode %d\n", inode_index);
            ret = -1;
            break;
        }
        offset += written;
    }
    // Free the cache.
    ext2_dealloc_cache(cache);
    return ret;
}
