/// @file ext2.c
/// @brief EXT2 module registration: mount, and the VFS operation tables.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[EXT2  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "ext2_internal.h"

#include "fs/ext2.h"

// Used before it is defined below.
static vfs_file_t *ext2_mount_callback(const char *path, const char *device);

// ============================================================================
// Virtual FileSystem (VFS) operation tables
// ============================================================================

/// Filesystem general operations.
vfs_sys_operations_t ext2_sys_operations = {
    .mkdir_f   = ext2_mkdir,
    .rmdir_f   = ext2_rmdir,
    .stat_f    = ext2_stat,
    .statfs_f  = ext2_statfs,
    .creat_f   = ext2_creat,
    .symlink_f = NULL,
    .setattr_f = ext2_setattr,
};

/// Filesystem file operations.
vfs_file_operations_t ext2_fs_operations = {
    .open_f     = ext2_open,
    .unlink_f   = ext2_unlink,
    .close_f    = ext2_close,
    .read_f     = ext2_read,
    .write_f    = ext2_write,
    .lseek_f    = ext2_lseek,
    .stat_f     = ext2_fstat,
    .ioctl_f    = ext2_ioctl,
    .getdents_f = ext2_getdents,
    .readlink_f = ext2_readlink,
    .setattr_f  = ext2_fsetattr,
};

/// Filesystem information.
static file_system_type_t ext2_file_system_type = {.name = "ext2", .fs_flags = 0, .mount = ext2_mount_callback};

#ifdef ENABLE_EXT2_TRACE
/// @brief Tracks the unique ID of the currently registered resource.
int ext2_resource_id = -1;
#endif

/// @brief Mounts the block device as an EXT2 filesystem.
/// @param block_device the block device formatted as EXT2.
/// @param path location where we mount the filesystem.
/// @return the VFS root node of the EXT2 filesystem.
static vfs_file_t *ext2_mount(vfs_file_t *block_device, const char *path)
{
    pr_debug("ext2_mount(device: %s, path: %s)\n", block_device->name, path);
    // Create the ext2 filesystem.
    ext2_filesystem_t *fs = kmalloc(sizeof(ext2_filesystem_t));
    // Clean the memory.
    memset(fs, 0, sizeof(ext2_filesystem_t));
    // Initialize the filesystem spinlock.
    spinlock_init(&fs->spinlock);
    // Initialize the list of opened files.
    list_head_init(&fs->opened_files);
    // Set the pointer to the block device.
    fs->block_device = block_device;
    // Read the superblock.
    if (ext2_read_superblock(fs) == -1) {
        pr_err("Failed to read the superblock table at 1024.\n");
        // Free just the filesystem.
        goto free_filesystem;
    }
    // Check the superblock magic number.
    if (fs->superblock.magic != EXT2_SUPERBLOCK_MAGIC) {
        pr_err("Wrong magic number, it is not an EXT2 filesystem.\n");
        ext2_dump_superblock(&fs->superblock);
        // Free just the filesystem.
        goto free_filesystem;
    }
    // Compute the volume size.
    fs->block_size = 1024U << fs->superblock.log_block_size;
    // Initialize the buffer cache.
    fs->ext2_buffer_cache =
        kmem_cache_create("ext2_buffer_cache", fs->block_size, fs->block_size, GFP_KERNEL, NULL, NULL);
    if (fs->ext2_buffer_cache == NULL) {
        pr_err("Failed to create buffer cache for ext2\n");
        goto free_filesystem;
    }

#ifdef ENABLE_EXT2_TRACE
    ext2_resource_id = register_resource("ext2");
#endif

    // uint8_t *caches[100];
    // for (size_t i = 0; i < 100; ++i) {
    //     caches[i] = ext2_alloc_cache(fs);
    // }
    // for (size_t i = 0; i < 100; ++i) {
    //     kmem_cache_free(caches[i]);
    // }

    // while (1) {}

    // Compute the maximum number of inodes per block.
    fs->inodes_per_block_count = fs->block_size / fs->superblock.inode_size;
    // Compute the number of blocks per block. This value is mostly used for
    // inodes.
    // If you check inside the inode structure you will find the `blocks_count`
    // field. A 32-bit value representing the total number of 512-bytes blocks
    // reserved to contain the data of this inode, regardless if these blocks
    // are used or not. The block numbers of these reserved blocks are contained
    // in the `block` array.
    // Since this value represents 512-byte blocks and not file system blocks,
    // this value should not be directly used as an index to the `block` array.
    // Rather, the maximum index of the `block` array should be computed from
    //      inode->blocks_count / ((1024 << superblock->log_block_size) / 512)
    // or once simplified:
    //      inode->blocks_count / (2 << superblock->log_block_size)
    // Now we just need to precompute the right part.
    fs->blocks_per_block_count = fs->block_size / 512U;
    // Compute the number of block pointers per block.
    fs->pointers_per_block     = fs->block_size / 4U;
    // Compute the number of block groups.
    fs->block_groups_count     = fs->superblock.blocks_count / fs->superblock.blocks_per_group;
    if (fs->superblock.blocks_per_group * fs->block_groups_count < fs->superblock.blocks_count) {
        fs->block_groups_count += 1;
    }
    // The block group descriptor table starts on the first block following the
    // superblock. This would be the second block for 2KiB and larger block file systems.
    if (fs->block_size > K) {
        fs->bgdt_start_block = 1;
    } else {
        // However, it would be the third block on a 1KiB block file system.
        fs->bgdt_start_block = 2;
    }
    // The block group descriptor table ends a certain amount of blocks.
    fs->bgdt_end_block =
        fs->bgdt_start_block + ((sizeof(ext2_group_descriptor_t) * fs->block_groups_count) / fs->block_size) + 1;
    // Compute the length in blocks of the BGDT.
    fs->bgdt_length = fs->bgdt_end_block - fs->bgdt_start_block;

    // Now, we have the size of a block, calculate the location of the Block
    // Group Descriptor Table (BGDT). The BGDT is located directly after the
    // superblock, so obtain the block of the superblock first.
    fs->block_groups = kmalloc(fs->block_size * fs->bgdt_length);
    if (fs->block_groups == NULL) {
        pr_err("Failed to allocate memory for the block buffer.\n");
        // Free just the filesystem.
        goto free_filesystem;
    }

    // Try to read the BGDT.
    if (ext2_read_bgdt(fs) == -1) {
        pr_err("Failed to read the BGDT.\n");
        // Free the block_groups and the filesystem.
        goto free_block_groups;
    }

    // We need the root inode in order to set the root file.
    ext2_inode_t root_inode;
    if (ext2_read_inode(fs, &root_inode, 2U) == -1) {
        pr_err("Failed to set the root inode.\n");
        // Free the block_buffer, the block_groups and the filesystem.
        goto free_block_buffer;
    }
    if ((root_inode.mode & S_IFDIR) != S_IFDIR) {
        pr_err("The root is not a directory.\n");
        // Free the block_buffer, the block_groups and the filesystem.
        goto free_block_buffer;
    }
    // Allocate the memory for the root.
    fs->root = vfs_alloc_file();
    if (!fs->root) {
        pr_err("Failed to allocate memory for the EXT2 root file!\n");
        // Free the block_buffer, the block_groups and the filesystem.
        goto free_block_buffer;
    }
    if (ext2_init_vfs_file(fs, fs->root, &root_inode, 2, path, strlen(path)) == -1) {
        pr_err("Failed to set the EXT2 root.\n");
        // Free the block_buffer, the block_groups and the filesystem.
        goto free_all;
    }
    // Set the count for the root to 1.
    fs->root->count = 1;
    // Add the root to the list of opened files.
    list_head_insert_before(&fs->root->siblings, &fs->opened_files);

    // Dump the filesystem details for debugging.
    ext2_dump_filesystem(fs);
    // Dump the superblock details for debugging.
    ext2_dump_superblock(&fs->superblock);
    // Dump the block group descriptor table.
    ext2_dump_bgdt(fs);

    return fs->root;

free_all:
    // Free the memory occupied by the root.
    vfs_dealloc_file(fs->root);
free_block_buffer:
    // Free the memory occupied by the block buffer.
    kmem_cache_destroy(fs->ext2_buffer_cache);
free_block_groups:
    // Free the memory occupied by the block groups.
    kfree(fs->block_groups);
free_filesystem:
    // Free the memory occupied by the filesystem.
    kfree(fs);
    return NULL;
}

/// @brief The mount call-back, which prepares everything and calls the actual
/// EXT2 mount function.
/// @param path the path where the filesystem should be mounted.
/// @param device the device we mount.
/// @return the VFS file of the filesystem.
static vfs_file_t *ext2_mount_callback(const char *path, const char *device)
{
    super_block_t *sb = vfs_get_superblock(device);
    if (sb == NULL) {
        pr_err("mount_callback(%s, %s): Cannot find the superblock (%s)!\n", path, device, device);
        vfs_dump_superblocks(LOGLEVEL_ERR);
        return NULL;
    }
    vfs_file_t *block_device = sb->root;
    if (block_device == NULL) {
        pr_err("mount_callback(%s, %s): Cannot find the superblock root.", path, device);
        return NULL;
    }
    if (block_device->flags != DT_BLK) {
        pr_err("mount_callback(%s, %s): The device is not a block device.\n", path, device);
        return NULL;
    }
    return ext2_mount(block_device, path);
}

int ext2_initialize(void)
{
    // Register the filesystem.
    vfs_register_filesystem(&ext2_file_system_type);
    return 0;
}

int ext2_finalize(void)
{
    vfs_unregister_filesystem(&ext2_file_system_type);
    return 0;
}
