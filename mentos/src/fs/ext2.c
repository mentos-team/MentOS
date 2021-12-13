/// @file ext2.c
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief EXT2 driver.
/// @version 0.1
/// @date 2021-12-13
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fs/ext2.h"

#define __DEBUG_HEADER__ "[EXT2  ]"
#define __DEBUG_LEVEL__  100

#include "process/scheduler.h"
#include "process/process.h"
#include "klib/spinlock.h"
#include "fs/vfs_types.h"
#include "sys/errno.h"
#include "io/debug.h"
#include "fs/vfs.h"
#include "string.h"
#include "stdio.h"
#include "fcntl.h"

/// @brief The superblock contains all the information about the configuration
/// of the filesystem.
/// @details The primary copy of the superblock is stored at an offset of 1024
/// bytes from the start of the device, and it is essential to mounting the
/// filesystem. Since it is so important, backup copies of the superblock are
/// stored in block groups throughout the filesystem.
typedef struct ext2_superblock_t {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t r_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
    // EXT2_DYNAMIC_REV
    uint32_t first_ino;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t uuid[16];
    uint8_t volume_name[16];
    uint8_t last_mounted[64];
    uint32_t algo_bitmap;
    // Performance Hints
    uint8_t prealloc_blocks;
    uint8_t prealloc_dir_blocks;
    uint16_t _padding;
    // Journaling Support
    uint8_t journal_uuid[16];
    uint32_t journal_inum;
    uint32_t jounral_dev;
    uint32_t last_orphan;
    // Directory Indexing Support
    uint32_t hash_seed[4];
    uint8_t def_hash_version;
    uint16_t _padding_a;
    uint8_t _padding_b;
    // Other Options
    uint32_t default_mount_options;
    uint32_t first_meta_bg;
    uint8_t _unused[760];
} __attribute__((packed)) ext2_superblock_t;

static const char *uuid_to_string(uint8_t uuid[16])
{
    static char s[33] = { 0 };
    sprintf(s, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
            uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
    return s;
}

static const char *time_to_string(uint32_t time)
{
    static char s[250] = { 0 };
    tm_t *tm           = localtime(&time);
    sprintf(s, "%2d/%2d %2d:%2d", tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min);
    return s;
}

static void ext2_dump_superblock(ext2_superblock_t *sb)
{
    pr_debug("inodes_count          : %d\n", sb->inodes_count);
    pr_debug("blocks_count          : %d\n", sb->blocks_count);
    pr_debug("r_blocks_count        : %d\n", sb->r_blocks_count);
    pr_debug("free_blocks_count     : %d\n", sb->free_blocks_count);
    pr_debug("free_inodes_count     : %d\n", sb->free_inodes_count);
    pr_debug("first_data_block      : %d\n", sb->first_data_block);
    pr_debug("log_block_size        : %d\n", sb->log_block_size);
    pr_debug("log_frag_size         : %d\n", sb->log_frag_size);
    pr_debug("blocks_per_group      : %d\n", sb->blocks_per_group);
    pr_debug("frags_per_group       : %d\n", sb->frags_per_group);
    pr_debug("inodes_per_group      : %d\n", sb->inodes_per_group);
    pr_debug("mtime                 : %s\n", time_to_string(sb->mtime));
    pr_debug("wtime                 : %s\n", time_to_string(sb->wtime));
    pr_debug("mnt_count             : %d\n", sb->mnt_count);
    pr_debug("max_mnt_count         : %d\n", sb->max_mnt_count);
    pr_debug("magic                 : %d\n", sb->magic);
    pr_debug("state                 : %d\n", sb->state);
    pr_debug("errors                : %d\n", sb->errors);
    pr_debug("minor_rev_level       : %d\n", sb->minor_rev_level);
    pr_debug("lastcheck             : %s\n", time_to_string(sb->lastcheck));
    pr_debug("checkinterval         : %d\n", sb->checkinterval);
    pr_debug("creator_os            : %d\n", sb->creator_os);
    pr_debug("rev_level             : %d\n", sb->rev_level);
    pr_debug("def_resuid            : %d\n", sb->def_resuid);
    pr_debug("def_resgid            : %d\n", sb->def_resgid);
    pr_debug("first_ino             : %d\n", sb->first_ino);
    pr_debug("inode_size            : %d\n", sb->inode_size);
    pr_debug("block_group_nr        : %d\n", sb->block_group_nr);
    pr_debug("feature_compat        : %d\n", sb->feature_compat);
    pr_debug("feature_incompat      : %d\n", sb->feature_incompat);
    pr_debug("feature_ro_compat     : %d\n", sb->feature_ro_compat);
    pr_debug("uuid                  : %s\n", uuid_to_string(sb->uuid));
    pr_debug("volume_name           : %s\n", (char *)sb->volume_name);
    pr_debug("last_mounted          : %s\n", (char *)sb->last_mounted);
    pr_debug("algo_bitmap           : %d\n", sb->algo_bitmap);
    pr_debug("prealloc_blocks       : %d\n", sb->prealloc_blocks);
    pr_debug("prealloc_dir_blocks   : %d\n", sb->prealloc_dir_blocks);
    pr_debug("_padding              : %d\n", sb->_padding);
    pr_debug("journal_uuid          : %s\n", uuid_to_string(sb->journal_uuid));
    pr_debug("journal_inum          : %d\n", sb->journal_inum);
    pr_debug("jounral_dev           : %d\n", sb->jounral_dev);
    pr_debug("last_orphan           : %d\n", sb->last_orphan);
    pr_debug("hash_seed             : %u %u %u %u\n", sb->hash_seed[0], sb->hash_seed[1], sb->hash_seed[2], sb->hash_seed[3]);
    pr_debug("def_hash_version      : %d\n", sb->def_hash_version);
    pr_debug("_padding_a            : %d\n", sb->_padding_a);
    pr_debug("_padding_b            : %d\n", sb->_padding_b);
    pr_debug("default_mount_options : %d\n", sb->default_mount_options);
    pr_debug("first_meta_bg         : %d\n", sb->first_meta_bg);
}

static vfs_file_t *ext2_mount_callback(const char *path, const char *device)
{
    vfs_file_t *dev = vfs_open(device, 0, 0);
    if (dev && (dev->flags == DT_BLK)) {
        pr_debug("Correctly found block device.\n");
        ext2_superblock_t sb;
        memset(&sb, 0, sizeof(ext2_superblock_t));
        if (vfs_read(dev, &sb, 1024, sizeof(ext2_superblock_t)) == -1) {
            pr_err("Failed to read the superblock table at 1024.\n");
            return NULL;
        }
        ext2_dump_superblock(&sb);
    }
    return NULL;
}

/// Filesystem general operations.
static vfs_sys_operations_t ext2_sys_operations = {
    .mkdir_f = NULL,
    .rmdir_f = NULL,
    .stat_f  = NULL
};

/// Filesystem file operations.
static vfs_file_operations_t ext2_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = NULL,
    .write_f    = NULL,
    .lseek_f    = NULL,
    .stat_f     = NULL,
    .ioctl_f    = NULL,
    .getdents_f = NULL
};

/// Filesystem information.
static file_system_type ext2_file_system_type = {
    .name     = "ext2",
    .fs_flags = 0,
    .mount    = ext2_mount_callback
};

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