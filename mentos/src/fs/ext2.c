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

#define EXT2_SUPERBLOCK_MAGIC  0xEF53 ///< Magic value used to identify an ext2 filesystem.
#define EXT2_INDIRECT_BLOCKS   12     ///< Amount of indirect blocks in an inode.
#define EXT2_PATH_MAX          4096   ///< Maximum length of a pathname.
#define EXT2_MAX_SYMLINK_COUNT 8      ///< Maximum nesting of symlinks, used to prevent a loop.
#define EXT2_NAME_LEN          255    ///< The lenght of names inside directory entries.

// File types.
#define EXT2_S_IFMT   0xF000 ///< Format mask
#define EXT2_S_IFSOCK 0xC000 ///< Socket
#define EXT2_S_IFLNK  0xA000 ///< Symbolic link
#define EXT2_S_IFREG  0x8000 ///< Regular file
#define EXT2_S_IFBLK  0x6000 ///< Block device
#define EXT2_S_IFDIR  0x4000 ///< Directory
#define EXT2_S_IFCHR  0x2000 ///< Character device
#define EXT2_S_IFIFO  0x1000 ///< Fifo

// Permissions bit.
#define EXT2_S_ISUID 0x0800 ///< SUID
#define EXT2_S_ISGID 0x0400 ///< SGID
#define EXT2_S_ISVTX 0x0200 ///< Sticky Bit
#define EXT2_S_IRWXU 0x01C0 ///< rwx------- : User can read/write/execute
#define EXT2_S_IRUSR 0x0100 ///< -r-------- : User can read
#define EXT2_S_IWUSR 0x0080 ///< --w------- : User can write
#define EXT2_S_IXUSR 0x0040 ///< ---x------ : User can execute
#define EXT2_S_IRWXG 0x0038 ///< ----rwx--- : Group can read/write/execute
#define EXT2_S_IRGRP 0x0020 ///< ----r----- : Group can read
#define EXT2_S_IWGRP 0x0010 ///< -----w---- : Group can write
#define EXT2_S_IXGRP 0x0008 ///< ------x--- : Group can execute
#define EXT2_S_IRWXO 0x0007 ///< -------rwx : Others can read/write/execute
#define EXT2_S_IROTH 0x0004 ///< -------r-- : Others can read
#define EXT2_S_IWOTH 0x0002 ///< --------w- : Others can write
#define EXT2_S_IXOTH 0x0001 ///< ---------x : Others can execute

typedef enum ext2_block_status_t {
    ext2_block_status_free     = 0, ///< The block is free.
    ext2_block_status_occupied = 1  ///< The block is occupied.
} ext2_block_status_t;

/// @brief The superblock contains all the information about the configuration
/// of the filesystem.
/// @details The primary copy of the superblock is stored at an offset of 1024
/// bytes from the start of the device, and it is essential to mounting the
/// filesystem. Since it is so important, backup copies of the superblock are
/// stored in block groups throughout the filesystem.
typedef struct ext2_superblock_t {
    /// @brief Total number of inodes in file system.
    uint32_t inodes_count;
    /// @brief Total number of blocks in file system
    uint32_t blocks_count;
    /// @brief Number of blocks reserved for superuser.
    uint32_t r_blocks_count;
    /// @brief Total number of unallocated blocks.
    uint32_t free_blocks_count;
    /// @brief Total number of unallocated inodes.
    uint32_t free_inodes_count;
    /// @brief Block number of the block containing the superblock.
    uint32_t first_data_block;
    /// @brief The number to shift 1024 to the left by to obtain the block size
    /// (log2 (block size) - 10).
    uint32_t log_block_size;
    /// @brief The number to shift 1024 to the left by to obtain the fragment
    /// size (log2 (fragment size) - 10).
    uint32_t log_frag_size;
    /// @brief Number of blocks in each block group.
    uint32_t blocks_per_group;
    /// @brief Number of fragments in each block group.
    uint32_t frags_per_group;
    /// @brief Number of inodes in each block group.
    uint32_t inodes_per_group;
    /// @brief Last mount time (in POSIX time).
    uint32_t mtime;
    /// @brief Last written time (in POSIX time).
    uint32_t wtime;
    /// @brief Number of times the volume has been mounted since its last
    /// consistency check (fsck).
    uint16_t mnt_count;
    /// @brief Number of mounts allowed before a consistency check (fsck) must
    /// be done.
    uint16_t max_mnt_count;
    /// @brief Ext2 signature (0xef53), used to help confirm the presence of
    /// Ext2 on a volume.
    uint16_t magic;
    /// @brief File system state.
    uint16_t state;
    /// @brief What to do when an error is detected.
    uint16_t errors;
    /// @brief Minor portion of version (combine with Major portion below to
    /// construct full version field).
    uint16_t minor_rev_level;
    /// @brief POSIX time of last consistency check (fsck).
    uint32_t lastcheck;
    /// @brief Interval (in POSIX time) between forced consistency checks
    /// (fsck).
    uint32_t checkinterval;
    /// @brief Operating system ID from which the filesystem on this volume was
    /// created.
    uint32_t creator_os;
    /// @brief Major portion of version (combine with Minor portion above to
    /// construct full version field).
    uint32_t rev_level;
    /// @brief User ID that can use reserved blocks.
    uint16_t def_resuid;
    /// @brief Group ID that can use reserved blocks.
    uint16_t def_resgid;

    // == Extended Superblock Fields ==========================================
    /// @brief First non-reserved inode in file system. (In versions < 1.0, this
    /// is fixed as 11)
    uint32_t first_ino;
    /// @brief Size of each inode structure in bytes. (In versions < 1.0, this
    /// is fixed as 128)
    uint16_t inode_size;
    /// @brief Block group that this superblock is part of (if backup copy).
    uint16_t block_group_nr;
    /// @brief Optional features present (features that are not required to read
    /// or write, but usually result in a performance increase).
    uint32_t feature_compat;
    /// @brief Required features present (features that are required to be
    /// supported to read or write)
    uint32_t feature_incompat;
    /// @brief Features that if not supported, the volume must be mounted
    /// read-only).
    uint32_t feature_ro_compat;
    /// @brief File system ID (what is output by blkid).
    uint8_t uuid[16];
    /// @brief Volume name (C-style string: characters terminated by a 0 byte).
    uint8_t volume_name[16];
    /// @brief Path volume was last mounted to (C-style string: characters
    /// terminated by a 0 byte).
    uint8_t last_mounted[64];
    /// @brief Compression algorithms used.
    uint32_t algo_bitmap;

    // == Performance Hints ===================================================
    /// @brief Number of blocks to preallocate for files.
    uint8_t prealloc_blocks;
    /// @brief Number of blocks to preallocate for directories.
    uint8_t prealloc_dir_blocks;
    /// @brief (Unused)
    uint16_t padding0;

    // == Journaling Support ==================================================
    /// @brief Journal ID
    uint8_t journal_uuid[16];
    /// @brief Inode number of journal file.
    uint32_t journal_inum;
    /// @brief Device number of journal file.
    uint32_t jounral_dev;
    /// @brief Start of list of inodes to delete.
    uint32_t last_orphan;

    // == Directory Indexing Support ==========================================
    /// @brief HTree hash seed.
    uint32_t hash_seed[4];
    /// @brief Ddefault hash version to use.
    uint8_t def_hash_version;
    /// @brief Padding.
    uint16_t padding1;
    /// @brief Padding.
    uint8_t padding2;

    // == Other Options =======================================================
    /// @brief The default mount options for the file system.
    uint32_t default_mount_options;
    /// @brief The ID of the first meta block group.
    uint32_t first_meta_block_group_id;
    /// @brief Reserved.
    uint8_t reserved[760];
} ext2_superblock_t;

/// @brief
typedef struct ext2_group_descriptor_t {
    /// @brief The block number of the block bitmap for this Block Group
    uint32_t block_bitmap;
    /// @brief The block number of the inode allocation bitmap for this Block Group.
    uint32_t inode_bitmap;
    /// @brief The block number of the starting block for the inode table for this Block Group.
    uint32_t inode_table;
    /// @brief Number of free blocks.
    uint16_t free_blocks_count;
    /// @brief Number of free inodes.
    uint16_t free_inodes_count;
    /// @brief Number of used directories.
    uint16_t used_dirs_count;
    /// @brief Padding.
    uint16_t pad;
    /// @brief Reserved.
    uint32_t reserved[3];
} ext2_group_descriptor_t;

/// @brief The ext2 inode.
typedef struct ext2_inode_t {
    /// @brief File mode
    uint16_t mode;
    /// @brief The user identifiers of the owners.
    uint16_t uid;
    /// @brief The size of the file in bytes.
    uint32_t size;
    /// @brief The time that the inode was accessed.
    uint32_t atime;
    /// @brief The time that the inode was created.
    uint32_t ctime;
    /// @brief The time that the inode was modified the last time.
    uint32_t mtime;
    /// @brief The time that the inode was deleted.
    uint32_t dtime;
    /// @brief The group identifiers of the owners.
    uint16_t gid;
    /// @brief Number of hard links.
    uint16_t links_count;
    /// @brief Blocks count.
    uint32_t blocks_count;
    /// @brief File flags.
    uint32_t flags;
    /// @brief OS dependant value.
    uint32_t osd1;
    /// @brief
    union blocks_t {
        /// [60 byte]
        struct blocks_data_t {
            /// [48 byte]
            uint32_t dir_blocks[EXT2_INDIRECT_BLOCKS];
            /// [ 4 byte]
            uint32_t indir_block;
            /// [ 4 byte]
            uint32_t doubly_indir_block;
            /// [ 4 byte]
            uint32_t trebly_indir_block;
        } blocks;
        /// [60 byte]
        char symlink[60];
    } data;
    /// @brief Value used to indicate the file version (used by NFS).
    uint32_t generation;
    /// @brief Value indicating the block number containing the extended attributes.
    uint32_t file_acl;
    /// @brief For regular files this 32bit value contains the high 32 bits of the 64bit file size.
    uint32_t dir_acl;
    /// @brief Value indicating the location of the file fragment.
    uint32_t fragment_addr;
    /// @brief OS dependant structure.
    uint32_t osd2[3];
} ext2_inode_t;

/// @brief The header of an ext2 directory entry.
typedef struct ext2_dirent_t {
    /// @brief Inode number.
    uint32_t inode;
    /// @brief Directory entry length
    uint16_t direntlen;
    /// @brief Name length
    uint8_t namelen;
    /// @brief File type.
    uint8_t filetype;
    /// @brief File name.
    char name[EXT2_NAME_LEN];
} ext2_dirent_t;

/// @brief The details regarding the filesystem.
typedef struct ext2_filesystem_t {
    /// Pointer to the block device.
    vfs_file_t *block_device;
    /// Device superblock, contains important information.
    ext2_superblock_t superblock;
    /// Block Group Descriptor / Block groups.
    ext2_group_descriptor_t *block_groups;
    /// A buffer the size of a block.
    char *block_buffer;
    /// A buffer the size of an inode.
    char *inode_buffer;
    /// Root FS node (attached to mountpoint).
    vfs_file_t *root;

    /// Size of one block.
    unsigned int block_size;

    /// Number of inodes that fit in a block.
    unsigned int inodes_per_block_count;
    /// Number of blocks that fit in a block.
    unsigned int blocks_per_block_count;
    /// Number of blocks groups.
    unsigned int block_groups_count;
    /// Number of block pointers per block.
    unsigned int pointers_per_block;
    /// Index in terms of blocks where the BGDT starts.
    unsigned int bgdt_start_block;
    /// Index in terms of blocks where the BGDT ends.
    unsigned int bgdt_end_block;
    /// The number of blocks containing the BGDT
    unsigned int bgdt_length;

    /// Index of indirect blocks.
    unsigned int indirect_blocks_index;
    /// Index of doubly-indirect blocks.
    unsigned int doubly_indirect_blocks_index;
    /// Index of trebly-indirect blocks.
    unsigned int trebly_indirect_blocks_index;

    /// Spinlock for protecting filesystem operations.
    spinlock_t spinlock;
} ext2_filesystem_t;

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

static inline void ext2_dump_superblock(ext2_superblock_t *sb)
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
    pr_debug("magic                 : 0x%0x\n", sb->magic);
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
    pr_debug("journal_uuid          : %s\n", uuid_to_string(sb->journal_uuid));
    pr_debug("journal_inum          : %d\n", sb->journal_inum);
    pr_debug("jounral_dev           : %d\n", sb->jounral_dev);
    pr_debug("last_orphan           : %d\n", sb->last_orphan);
    pr_debug("hash_seed             : %u %u %u %u\n", sb->hash_seed[0], sb->hash_seed[1], sb->hash_seed[2], sb->hash_seed[3]);
    pr_debug("def_hash_version      : %d\n", sb->def_hash_version);
    pr_debug("default_mount_options : %d\n", sb->default_mount_options);
    pr_debug("first_meta_bg         : %d\n", sb->first_meta_block_group_id);
}

static inline void ext2_dump_group_descriptor(ext2_group_descriptor_t *gd)
{
    pr_debug("block_bitmap          : %d\n", gd->block_bitmap);
    pr_debug("inode_bitmap          : %d\n", gd->inode_bitmap);
    pr_debug("inode_table           : %d\n", gd->inode_table);
    pr_debug("free_blocks_count     : %d\n", gd->free_blocks_count);
    pr_debug("free_inodes_count     : %d\n", gd->free_inodes_count);
    pr_debug("used_dirs_count       : %d\n", gd->used_dirs_count);
}

static inline void ext2_dump_filesystem(ext2_filesystem_t *fs)
{
    pr_debug("block_device          : 0x%x\n", fs->block_device);
    pr_debug("superblock            : 0x%x\n", fs->superblock);
    pr_debug("block_buffer          : 0x%x\n", fs->block_buffer);
    pr_debug("inode_buffer          : 0x%x\n", fs->inode_buffer);
    pr_debug("block_groups          : 0x%x\n", fs->block_groups);
    pr_debug("root                  : 0x%x\n", fs->root);
    pr_debug("block_size            : %d\n", fs->block_size);
    pr_debug("inodes_per_block_count: %d\n", fs->inodes_per_block_count);
    pr_debug("blocks_per_block_count: %d\n", fs->blocks_per_block_count);
    pr_debug("block_groups_count    : %d\n", fs->block_groups_count);
    pr_debug("pointers_per_block    : %d\n", fs->pointers_per_block);
    pr_debug("bgdt_start_block      : %d\n", fs->bgdt_start_block);
    pr_debug("bgdt_end_block        : %d\n", fs->bgdt_end_block);
    pr_debug("bgdt_length           : %d\n", fs->bgdt_length);
}

/// @brief Cheks if the bit at the given index is free.
/// @param block_buffer the buffer containing the bitmap
/// @param index the index we want to check.
/// @return if the bit is 0 or 1.
/// @details
/// How we access the specific bits inside the bitmap takes inspiration from the
/// mailman's algorithm.
static inline bool_t ext2_check_block_bit(char *block_buffer, unsigned int index)
{
    return bit_check(block_buffer[index / 8], index % 8) != 0;
}

/// @brief Sets the bit at the given index accordingly to `status`.
/// @param block_buffer the buffer containing the bitmap
/// @param index the index we want to check.
/// @param status the new status of the block (free|occupied).
static inline void ext2_set_block_bit(char *block_buffer, unsigned int index, ext2_block_status_t status)
{
    if (status == ext2_block_status_occupied)
        bit_set_assign(block_buffer[index / 8], index % 8);
    else
        bit_clear_assign(block_buffer[index / 8], index % 8);
}

/// @brief Reads the superblock from the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return the amount of data we read, or negative value for an error.
static inline int ext2_read_superblock(ext2_filesystem_t *fs)
{
    pr_debug("Read superblock for EXT2 filesystem (0x%x)\n", fs);
    return vfs_read(fs->block_device, &fs->superblock, 1024, sizeof(super_block_t));
}

/// @brief Writes the superblock on the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return the amount of data we wrote, or negative value for an error.
static inline int ext2_write_superblock(ext2_filesystem_t *fs)
{
    pr_debug("Write superblock for EXT2 filesystem (0x%x)\n", fs);
    return vfs_write(fs->block_device, &fs->superblock, 1024, sizeof(super_block_t));
}

/// @brief Read a block from the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @param block_index the index of the block we want to read.
/// @param buffer the buffer where the content will be placed.
/// @return the amount of data we read, or negative value for an error.
static inline int ext2_read_block(ext2_filesystem_t *fs, unsigned int block_index, char *buffer)
{
    pr_debug("Read block %4d for EXT2 filesystem (0x%x)\n", block_index, fs);
    if (block_index == 0) {
        pr_err("You are trying to read an invalid block index (%d).\n", block_index);
        return -1;
    }
    if (buffer == NULL) {
        pr_err("You are trying to read with a NULL buffer.\n");
        return -1;
    }
    return vfs_read(fs->block_device, buffer, block_index * fs->block_size, fs->block_size);
}

/// @brief Writes a block on the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @param block_index the index of the block we want to read.
/// @param buffer the buffer where the content will be placed.
/// @return the amount of data we wrote, or negative value for an error.
static inline int ext2_write_block(ext2_filesystem_t *fs, unsigned int block_index, char *buffer)
{
    pr_debug("Write block %4d for EXT2 filesystem (0x%x)\n", block_index, fs);
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
static inline int ext2_read_bgdt(ext2_filesystem_t *fs)
{
    pr_debug("Read BGDT for EXT2 filesystem (0x%x)\n", fs);
    if (fs->block_groups) {
        for (unsigned i = 0; i < fs->bgdt_length; ++i)
            ext2_read_block(fs, fs->bgdt_start_block + i, (char *)((uintptr_t)fs->block_groups + (fs->block_size * i)));
        return 0;
    }
    pr_err("The `block_groups` list is not initialized.\n");
    return -1;
}

/// @brief Writes the Block Group Descriptor Table (BGDT) to the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return 0 on success, -1 on failure.
static inline int ext2_write_bgdt(ext2_filesystem_t *fs)
{
    pr_debug("Write BGDT for EXT2 filesystem (0x%x)\n", fs);
    if (fs->block_groups) {
        for (unsigned i = 0; i < fs->bgdt_length; ++i)
            ext2_write_block(fs, fs->bgdt_start_block + i, (char *)((uintptr_t)fs->block_groups + (fs->block_size * i)));
        return 0;
    }
    pr_err("The `block_groups` list is not initialized.\n");
    return -1;
}

static inline void ext2_dump_bgdt(ext2_filesystem_t *fs)
{
    for (uint32_t i = 0; i < fs->block_groups_count; ++i) {
        // Get the pointer to the current group descriptor.
        ext2_group_descriptor_t *gd = &(fs->block_groups[i]);

        pr_debug("Block Group Descriptor [%d] @ %d:\n", i, fs->bgdt_start_block + i * fs->superblock.blocks_per_group);
        pr_debug("    Free Blocks : %4d of %d\n", gd->free_blocks_count, fs->superblock.blocks_per_group);
        pr_debug("    Free Inodes : %4d of %d\n", gd->free_inodes_count, fs->superblock.inodes_per_group);

        // Dump the block bitmap.
        ext2_read_block(fs, gd->block_bitmap, fs->block_buffer);
        pr_debug("    Block Bitmap at %d\n", gd->block_bitmap);
        for (unsigned j = 0; j < fs->block_size; ++j) {
            if ((j % 8) == 0)
                pr_debug("        Block index: %4d, Bitmap: %s\n", j / 8, dec_to_binary(fs->block_buffer[j / 8], 8));
            if (!ext2_check_block_bit(fs->block_buffer, j)) {
                pr_debug("    First free block in group is in block %d, the linear index is %d\n", j / 8, j);
                break;
            }
        }

        // Dump the block bitmap.
        ext2_read_block(fs, gd->inode_bitmap, fs->block_buffer);
        pr_debug("    Inode Bitmap at %d\n", gd->inode_bitmap);
        for (unsigned j = 0; j < fs->block_size; ++j) {
            if ((j % 8) == 0)
                pr_debug("        Block index: %4d, Bitmap: %s\n", j / 8, dec_to_binary(fs->block_buffer[j / 8], 8));
            if (!ext2_check_block_bit(fs->block_buffer, j)) {
                pr_debug("    First free block in group is in block %d, the linear index is %d\n", j / 8, j);
                break;
            }
        }
    }
}

/// @brief Reads an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @return 0 on success, -1 on failure.
static inline int ext2_read_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, unsigned inode_index)
{
    if (inode_index == 0) {
        pr_err("You are trying to read an invalid inode index (%d).\n", inode_index);
        return -1;
    }
    // Update the inode index.
    inode_index -= 1;
    // Retrieve the group index.
    unsigned group_index = inode_index / fs->superblock.inodes_per_group;
    if (group_index > fs->block_groups_count) {
        pr_err("Invalid group index computed from inode index `%d`.\n", inode_index);
        return -1;
    }
    // Retrieve the group.
    ext2_group_descriptor_t *group_desc = &fs->block_groups[group_index];
    // Get the index of the inode inside the group.
    inode_index -= group_index * fs->superblock.inodes_per_group;
    // Get the block offest.
    unsigned block_offset = (inode_index * fs->superblock.inode_size) / fs->block_size;
    // Get the offset inside the block.
    uint32_t offset_in_block = inode_index - block_offset * (fs->block_size / fs->superblock.inode_size);
    // Read the block containing the inode table.
    ext2_read_block(fs, group_desc->inode_table + block_offset, fs->block_buffer);
    // Get the first entry inside the inode table.
    ext2_inode_t *inode_table = (ext2_inode_t *)fs->block_buffer;
    // Save the inode content.
    memcpy(inode, (uint8_t *)((uintptr_t)inode_table + offset_in_block * fs->superblock.inode_size), fs->superblock.inode_size);
    return 0;
}

/// @brief Writes the inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @return 0 on success, -1 on failure.
static int ext2_write_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, unsigned inode_index)
{
    if (inode_index == 0) {
        pr_err("You are trying to read an invalid inode index (%d).\n", inode_index);
        return -1;
    }
    // Update the inode index.
    inode_index -= 1;
    // Retrieve the group index.
    unsigned group_index = inode_index / fs->superblock.inodes_per_group;
    if (group_index > fs->block_groups_count) {
        pr_err("Invalid group index computed from inode index `%d`.\n", inode_index);
        return -1;
    }
    // Retrieve the group.
    ext2_group_descriptor_t *group_desc = &fs->block_groups[group_index];
    // Get the index of the inode inside the group.
    inode_index -= group_index * fs->superblock.inodes_per_group;
    // Get the block offest.
    unsigned block_offset = (inode_index * fs->superblock.inode_size) / fs->block_size;
    // Get the offset inside the block.
    uint32_t offset_in_block = inode_index - block_offset * (fs->block_size / fs->superblock.inode_size);
    // Read the block containing the inode table.
    ext2_read_block(fs, group_desc->inode_table + block_offset, fs->block_buffer);
    // Get the first entry inside the inode table.
    ext2_inode_t *inode_table = (ext2_inode_t *)fs->block_buffer;
    // Write the inode.
    memcpy((char *)((uintptr_t)inode_table + offset_in_block * fs->superblock.inode_size), inode, fs->superblock.inode_size);
    // Write back the block.
    ext2_write_block(fs, group_desc->inode_table + block_offset, (char *)inode_table);
    return 0;
}

/// @brief Allocates a new block.
/// @param fs the filesystem.
/// @return 0 on failure, or the index of the new block on success.
static uint32_t ext2_allocate_block(ext2_filesystem_t *fs)
{
    uint32_t group_index, block_index, linear_index;
    // Lock the filesystem.
    spinlock_lock(&fs->spinlock);
    // Get the group and bit index of the first free block.
    for (group_index = 0; group_index < fs->block_groups_count; ++group_index) {
        // Check if there are free blocks in this block group.
        if (fs->block_groups[group_index].free_blocks_count > 0) {
            // Read the block bitmap.
            ext2_read_block(fs, fs->block_groups[group_index].block_bitmap, fs->block_buffer);
            // Find the first free block.
            for (linear_index = 0; linear_index < fs->block_size; ++linear_index) {
                // We found a free block.
                if (!ext2_check_block_bit(fs->block_buffer, linear_index)) {
                    // Compute the block index.
                    block_index = (fs->superblock.blocks_per_group * group_index) + linear_index;
                    break;
                }
            }
            if (block_index != 0)
                break;
        }
    }
    // Check if we have found a free block.
    if (block_index == 0) {
        pr_err("Cannot find a free block.\n");
        // Unlock the spinlock.
        spinlock_unlock(&fs->spinlock);
        return 0;
    }
    // Set the block as occupied.
    ext2_set_block_bit(fs->block_buffer, linear_index, ext2_block_status_occupied);
    // Update the bitmap.
    ext2_write_block(fs, fs->block_groups[group_index].block_bitmap, fs->block_buffer);
    // Decrease the number of free blocks inside the BGDT entry.
    fs->block_groups[group_index].free_blocks_count -= 1;
    // Update the BGDT.
    if (ext2_write_bgdt(fs) == -1) {
        pr_err("Cannot allocate the block.\n");
        // Unlock the spinlock.
        spinlock_unlock(&fs->spinlock);
        return 0;
    }
    // Decrease the number of free blocks inside the superblock.
    fs->superblock.free_blocks_count -= 1;
    // Update the superblock.
    ext2_write_superblock(fs);
    // Empty out the new block.
    memset(fs->block_buffer, 0, fs->block_size);
    ext2_write_block(fs, block_index, fs->block_buffer);
    // Unlock the spinlock.
    spinlock_unlock(&fs->spinlock);
    return block_index;
}

/// @brief Sets the real block index based on the block index inside an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param block_index the block index inside the inode.
/// @param real_index the real block number.
/// @return 0 on success, a negative value on failure.
static int ext2_set_real_block_index(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index, uint32_t block_index, uint32_t real_index)
{
    // Set the direct block pointer.
    if (block_index < EXT2_INDIRECT_BLOCKS) {
        inode->data.blocks.dir_blocks[block_index] = real_index;
        return 0;
    }
    // Check if the index is among the indirect blocks.
    if (block_index < fs->indirect_blocks_index) {
        // Compute the indirect indices.
        uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;

        // Check that the indirect block points to a valid block.
        if (!inode->data.blocks.indir_block) {
            // Allocate a new block.
            unsigned int new_block_index = ext2_allocate_block(fs);
            if (new_block_index == 0)
                return -1;
            // Update the index.
            inode->data.blocks.indir_block = new_block_index;
            // Update the inode.
            if (ext2_write_inode(fs, inode, inode_index) == -1)
                return -1;
        }

        // Read the indirect block (which contains pointers to the next set of blocks).
        ext2_read_block(fs, inode->data.blocks.indir_block, fs->block_buffer);
        // Write the index inside the final block.
        fs->block_buffer[a] = real_index;
        // Write back the indirect block.
        ext2_read_block(fs, inode->data.blocks.indir_block, fs->block_buffer);
        return 0;
    }

    // For simplicity.
    uint32_t p1 = fs->pointers_per_block, p2 = fs->pointers_per_block * fs->pointers_per_block;

    // Check if the index is among the doubly-indirect blocks.
    if (block_index < fs->doubly_indirect_blocks_index) {
        // Compute the indirect indices.
        uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;
        uint32_t b = a - p1;
        uint32_t c = b / p1;
        uint32_t d = b - (c * p1);

        // Check that the indirect block points to a valid block.
        if (!inode->data.blocks.doubly_indir_block) {
            // Allocate a new block.
            unsigned int new_block_index = ext2_allocate_block(fs);
            if (new_block_index == 0)
                return -1;
            // Update the index.
            inode->data.blocks.doubly_indir_block = new_block_index;
            // Update the inode.
            if (ext2_write_inode(fs, inode, inode_index) == -1)
                return -1;
        }

        // Read the doubly-indirect block (which contains pointers to indirect blocks).
        ext2_read_block(fs, inode->data.blocks.doubly_indir_block, fs->block_buffer);
        // Check that the indirect block points to a valid block.
        if (!fs->block_buffer[c]) {
            // Allocate a new block.
            unsigned int new_block_index = ext2_allocate_block(fs);
            if (new_block_index == 0)
                return -1;
            // Update the index.
            fs->block_buffer[c] = new_block_index;
            // Write the doubly-indirect block back.
            ext2_write_block(fs, inode->data.blocks.doubly_indir_block, fs->block_buffer);
        }

        // Compute the index inside the indirect block.
        ext2_read_block(fs, (uint32_t)fs->block_buffer[c], fs->block_buffer);
        // Write the index inside the final block.
        fs->block_buffer[d] = real_index;
        // Write back the indirect block.
        ext2_read_block(fs, (uint32_t)fs->block_buffer[c], fs->block_buffer);
        return 0;
    }

    // Check if the index is among the trebly-indirect blocks.
    if (block_index < fs->trebly_indirect_blocks_index) {
        // Compute the indirect indices.
        uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;
        uint32_t b = a - p1;
        uint32_t c = b - p2;
        uint32_t d = c / p2;
        uint32_t e = c - (d * p2);
        uint32_t f = e / p1;
        uint32_t g = e - (f * p1);
        uint32_t block_index_save;

        // Check that the indirect block points to a valid block.
        if (!inode->data.blocks.trebly_indir_block) {
            // Allocate a new block.
            unsigned int new_block_index = ext2_allocate_block(fs);
            if (new_block_index == 0)
                return -1;
            // Update the index.
            inode->data.blocks.trebly_indir_block = new_block_index;
            // Update the inode.
            if (ext2_write_inode(fs, inode, inode_index) == -1)
                return -1;
        }

        // Read the trebly-indirect block (which contains pointers to doubly-indirect blocks).
        ext2_read_block(fs, inode->data.blocks.trebly_indir_block, fs->block_buffer);
        // Check that the indirect block points to a valid block.
        if (!fs->block_buffer[d]) {
            // Allocate a new block.
            unsigned int new_block_index = ext2_allocate_block(fs);
            if (new_block_index == 0)
                return -1;
            // Update the index.
            fs->block_buffer[d] = new_block_index;
            // Write the doubly-indirect block back.
            ext2_write_block(fs, inode->data.blocks.trebly_indir_block, fs->block_buffer);
        }
        // Save the block index, otherwise with the next read we lose the block index list [d].
        block_index_save = (uint32_t)fs->block_buffer[d];

        // Read the doubly-indirect block (which contains pointers to indirect blocks).
        ext2_read_block(fs, block_index_save, fs->block_buffer);
        // Check that the indirect block points to a valid block.
        if (!fs->block_buffer[f]) {
            // Allocate a new block.
            unsigned int new_block_index = ext2_allocate_block(fs);
            if (new_block_index == 0)
                return -1;
            // Update the index.
            fs->block_buffer[f] = new_block_index;
            // Write the doubly-indirect block back.
            ext2_write_block(fs, block_index_save, fs->block_buffer);
        }

        // Get the next group index.
        block_index_save = (uint32_t)fs->block_buffer[f];
        // Read the indirect block (which contains pointers to the next set of blocks).
        ext2_read_block(fs, block_index_save, fs->block_buffer);
        // Write the index inside the final block.
        fs->block_buffer[g] = real_index;
        // Write back the indirect block.
        ext2_read_block(fs, block_index_save, fs->block_buffer);
        return 0;
    }
    pr_err("We failed to write the real block number of the block with index `%d`\n", block_index);
    return -1;
}

/// @brief Returns the real block index starting from a block index inside an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param block_index the block index inside the inode.
/// @return the real block number.
static uint32_t ext2_get_real_block_index(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t block_index)
{
    // Return the direct block pointer.
    if (block_index < EXT2_INDIRECT_BLOCKS) {
        return inode->data.blocks.dir_blocks[block_index];
    }

    // Check if the index is among the indirect blocks.
    if (block_index < fs->indirect_blocks_index) {
        // Compute the indirect indices.
        uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;

        // Read the indirect block (which contains pointers to the next set of blocks).
        ext2_read_block(fs, inode->data.blocks.indir_block, fs->block_buffer);

        // Compute the index inside the final block.
        return (uint32_t)fs->block_buffer[a];
    }
    // For simplicity.
    uint32_t p1 = fs->pointers_per_block, p2 = fs->pointers_per_block * fs->pointers_per_block;

    // Check if the index is among the doubly-indirect blocks.
    if (block_index < fs->doubly_indirect_blocks_index) {
        // Compute the indirect indices.
        uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;
        uint32_t b = a - p1;
        uint32_t c = b / p1;
        uint32_t d = b - (c * p1);

        // Read the doubly-indirect block (which contains pointers to indirect blocks).
        ext2_read_block(fs, inode->data.blocks.doubly_indir_block, fs->block_buffer);

        // Compute the index inside the indirect block.
        ext2_read_block(fs, (uint32_t)fs->block_buffer[c], fs->block_buffer);

        // Compute the index inside the final block.
        return (uint32_t)fs->block_buffer[d];
    }

    // Check if the index is among the trebly-indirect blocks.
    if (block_index < fs->trebly_indirect_blocks_index) {
        // Compute the indirect indices.
        uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;
        uint32_t b = a - p1;
        uint32_t c = b - p2;
        uint32_t d = c / p2;
        uint32_t e = c - (d * p2);
        uint32_t f = e / p1;
        uint32_t g = e - (f * p1);

        // Read the trebly-indirect block (which contains pointers to doubly-indirect blocks).
        ext2_read_block(fs, inode->data.blocks.trebly_indir_block, fs->block_buffer);

        // Read the doubly-indirect block (which contains pointers to indirect blocks).
        ext2_read_block(fs, (uint32_t)fs->block_buffer[d], fs->block_buffer);

        // Read the indirect block (which contains pointers to the next set of blocks).
        ext2_read_block(fs, (uint32_t)fs->block_buffer[f], fs->block_buffer);

        // Compute the index inside the final block.
        return (uint32_t)fs->block_buffer[g];
    }
    pr_err("We failed to retrieve the real block number of the block with index `%d`\n", block_index);
    return 0;
}

/// @brief Allocate a new block for an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @param block_index The index of the block within the inode.
/// @return 0 on success, -1 on failure.
static int ext2_allocate_inode_block(ext2_filesystem_t *fs, ext2_inode_t *inode, unsigned int inode_index, unsigned int block_index)
{
    pr_debug("Allocating block with index `%d` for inode with index `%d`.\n", block_index, inode_index);
    // Allocate the block.
    int real_index = ext2_allocate_block(fs);
    if (real_index == -1)
        return -1;
    // Associate the real index and the index inside the inode.
    if (ext2_set_real_block_index(fs, inode, inode_index, block_index, real_index) == -1)
        return -1;
    // Compute the new blocks count.
    unsigned int blocks_count = (block_index + 1) * fs->blocks_per_block_count;
    if (inode->blocks_count < blocks_count) {
        inode->blocks_count = blocks_count;
        pr_debug("Setting the block count for inode to %d = (%d blocks)",
                 blocks_count, blocks_count / fs->blocks_per_block_count);
    }
    // Update the inode.
    if (ext2_write_inode(fs, inode, inode_index) == -1)
        return -1;
    return 0;
}

/// @brief Reads the real block starting from an inode and the block index inside the inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param block_index the index of the block within the inode.
/// @param buffer the buffer where to put the data.
/// @return the amount read.
static ssize_t ext2_read_inode_block(ext2_filesystem_t *fs, ext2_inode_t *inode, unsigned int block_index, char *buffer)
{
    if (block_index >= (inode->blocks_count / fs->blocks_per_block_count)) {
        pr_err("Tried to read an invalid block `%d`, but inode only has %d!",
               block_index, (inode->blocks_count / fs->blocks_per_block_count));
        return -1;
    }
    // Get the real index.
    unsigned int real_index = ext2_get_real_block_index(fs, inode, block_index);
    if (real_index == 0)
        return -1;
    // Read the block.
    return ext2_read_block(fs, real_index, buffer);
}

static ssize_t ext2_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte)
{
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem (%s).\n", file->name);
        return -1;
    }
    // Get the inode associated with the file.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, file->ino) == -1) {
        pr_err("Failed to read the inode (%s).\n", file->name);
        return -1;
    }
    // Check if the file is empty.
    if (inode.size == 0)
        return 0;

    uint32_t end;
    if ((offset + nbyte) > inode.size) {
        end = inode.size;
    } else {
        end = offset + nbyte;
    }
    uint32_t start_block  = offset / fs->block_size;
    uint32_t end_block    = end / fs->block_size;
    uint32_t end_size     = end - end_block * fs->block_size;
    uint32_t size_to_read = end - offset;

    if (start_block == end_block) {
        // Read the real block.
        ext2_read_inode_block(fs, &inode, start_block, fs->block_buffer);
        // Copy the content back to the buffer.
        memcpy(buffer, (uint8_t *)(((uintptr_t)fs->block_buffer) + ((uintptr_t)offset % fs->block_size)), size_to_read);
    } else {
        uint32_t block_offset;
        uint32_t blocks_read = 0;
        for (block_offset = start_block; block_offset < end_block; block_offset++, blocks_read++) {
            if (block_offset == start_block) {
                ext2_read_inode_block(fs, &inode, block_offset, fs->block_buffer);
                memcpy(buffer, (uint8_t *)(((uintptr_t)fs->block_buffer) + ((uintptr_t)offset % fs->block_size)), fs->block_size - (offset % fs->block_size));
            } else {
                ext2_read_inode_block(fs, &inode, block_offset, fs->block_buffer);
                memcpy(buffer + fs->block_size * blocks_read - (offset % fs->block_size), fs->block_buffer, fs->block_size);
            }
        }
        if (end_size) {
            ext2_read_inode_block(fs, &inode, end_block, fs->block_buffer);
            memcpy(buffer + fs->block_size * blocks_read - (offset % fs->block_size), fs->block_buffer, end_size);
        }
    }
    return size_to_read;
}

static int ext2_set_root(ext2_filesystem_t *fs, ext2_inode_t *inode)
{
    // Information for root dir.
    fs->root->device  = (void *)fs;
    fs->root->ino     = 2;
    fs->root->name[0] = '/';
    fs->root->name[1] = '\0';
    // Information from the inode.
    fs->root->uid    = inode->uid;
    fs->root->gid    = inode->gid;
    fs->root->length = inode->size;
    fs->root->mask   = inode->mode & 0xFFF;
    fs->root->nlink  = inode->links_count;
    // File flags.
    fs->root->flags = 0;
    if ((inode->mode & EXT2_S_IFREG) == EXT2_S_IFREG) {
        pr_err("The root inode is a regular file.\n");
        kmem_cache_free(fs->root);
        return -1;
    }
    if ((inode->mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
        fs->root->flags |= DT_DIR;
    } else {
        pr_err("The root inode is not a directory.\n");
        kmem_cache_free(fs->root);
        return -1;
    }
    if ((inode->mode & EXT2_S_IFBLK) == EXT2_S_IFBLK) {
        fs->root->flags |= DT_BLK;
    }
    if ((inode->mode & EXT2_S_IFCHR) == EXT2_S_IFCHR) {
        fs->root->flags |= DT_CHR;
    }
    if ((inode->mode & EXT2_S_IFIFO) == EXT2_S_IFIFO) {
        fs->root->flags |= DT_FIFO;
    }
    if ((inode->mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
        fs->root->flags |= DT_LNK;
    }
    fs->root->atime          = inode->atime;
    fs->root->mtime          = inode->mtime;
    fs->root->ctime          = inode->ctime;
    fs->root->sys_operations = NULL;
    fs->root->fs_operations  = NULL;
    list_head_init(&fs->root->siblings);
    return 0;
}

static vfs_file_t *ext2_mount(vfs_file_t *block_device)
{
    // Create the ext2 filesystem.
    ext2_filesystem_t *fs = kmalloc(sizeof(ext2_filesystem_t));
    // Clean the memory.
    memset(fs, 0, sizeof(ext2_filesystem_t));
    // Initialize the filesystem spinlock.
    spinlock_init(&fs->spinlock);
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
    // Compute the maximum number of inodes per block.
    fs->inodes_per_block_count = fs->block_size / sizeof(ext2_inode_t);
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
    fs->pointers_per_block = fs->block_size / 4U;
    // Compute the index of indirect blocks.
    fs->indirect_blocks_index        = EXT2_INDIRECT_BLOCKS + fs->pointers_per_block;
    fs->doubly_indirect_blocks_index = EXT2_INDIRECT_BLOCKS + fs->pointers_per_block * (fs->pointers_per_block + 1);
    fs->trebly_indirect_blocks_index = fs->doubly_indirect_blocks_index +
                                       (fs->pointers_per_block * fs->pointers_per_block) * (fs->pointers_per_block + 1);
    // Compute the number of block groups.
    fs->block_groups_count = fs->superblock.blocks_count / fs->superblock.blocks_per_group;
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
    fs->bgdt_end_block = fs->bgdt_start_block + ((sizeof(ext2_group_descriptor_t) * fs->block_groups_count) / fs->block_size) + 1;
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

    // Preentively allocate a buffer for reading blocks.
    fs->block_buffer = kmalloc(fs->block_size * sizeof(char));
    if (fs->block_buffer == NULL) {
        pr_err("Failed to allocate memory for the block buffer.\n");
        // Free the block_groups and the filesystem.
        goto free_block_groups;
    }
    memset(fs->block_buffer, 0, sizeof(char) * fs->block_size);

    // Preentively allocate a buffer for reading inodes.
    fs->inode_buffer = kmalloc(fs->superblock.inode_size * sizeof(char));
    if (fs->inode_buffer == NULL) {
        pr_err("Failed to allocate memory for the block buffer.\n");
        // Free the block_buffer, the block_groups and the filesystem.
        goto free_block_buffer;
    }
    memset(fs->inode_buffer, 0, sizeof(char) * fs->superblock.inode_size);

    // We need the root inode in order to set the root file.
    ext2_inode_t root_inode;
    if (ext2_read_inode(fs, &root_inode, 2U) == -1) {
        pr_err("Failed to set the root inode.\n");
        // Free the block_buffer, the block_groups and the filesystem.
        goto free_inode_buffer;
    }

    // Allocate the memory for the root.
    fs->root = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
    if (!fs->root) {
        pr_err("Failed to allocate memory for the EXT2 root file!\n");
        // Free the block_buffer, the block_groups and the filesystem.
        goto free_inode_buffer;
    }

    if (ext2_set_root(fs, &root_inode) == -1) {
        pr_err("Failed to set the EXT2 root.\n");
        // Free the block_buffer, the block_groups and the filesystem.
        goto free_all;
    }
    pr_notice("Mounted EXT2 disk, root VFS node is at 0x%x.\n", (uintptr_t)fs->root);

    // Dump the filesystem details for debugging.
    ext2_dump_filesystem(fs);
    // Dump the superblock details for debugging.
    ext2_dump_superblock(&fs->superblock);
    // Dump the block group descriptor table.
    ext2_dump_bgdt(fs);

free_all:
    // Free the memory occupied by the root.
    kmem_cache_free(fs->root);
free_inode_buffer:
    // Free the memory occupied by the inode buffer.
    kfree(fs->inode_buffer);
free_block_buffer:
    // Free the memory occupied by the block buffer.
    kfree(fs->block_buffer);
free_block_groups:
    // Free the memory occupied by the block groups.
    kfree(fs->block_groups);
free_filesystem:
    // Free the memory occupied by the filesystem.
    kfree(fs);
    return NULL;
}

static vfs_file_t *ext2_mount_callback(const char *path, const char *device)
{
    vfs_file_t *block_device = vfs_open(device, 0, 0);
    if (!block_device) {
        pr_warning("Can find the device at path `%s`.\n", path);
        return NULL;
    }
    if (block_device->flags != DT_BLK) {
        pr_warning("The device at path `%s` is not a block device.\n", path);
        vfs_close(block_device);
        return NULL;
    }
    return ext2_mount(block_device);
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