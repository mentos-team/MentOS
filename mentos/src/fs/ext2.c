/// @file ext2.c
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief EXT2 driver.
/// @version 0.1
/// @date 2021-12-13
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fs/ext2.h"

// Include the kernel log levels.
#include "sys/kernel_levels.h"
// Change the header.
#define __DEBUG_HEADER__ "[EXT2  ]"
// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_DEBUG

#include "process/scheduler.h"
#include "process/process.h"
#include "klib/spinlock.h"
#include "fs/vfs_types.h"
#include "sys/errno.h"
#include "io/debug.h"
#include "fs/vfs.h"
#include "assert.h"
#include "libgen.h"
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

// ============================================================================
// Data Structures
// ============================================================================

typedef enum ext2_file_type_t {
    ext2_file_type_unknown,          ///< Unknown type.
    ext2_file_type_regular_file,     ///< Regular file.
    ext2_file_type_directory,        ///< Directory.
    ext2_file_type_character_device, ///< Character device.
    ext2_file_type_block_device,     ///< Block device.
    ext2_file_type_named_pipe,       ///< Named pipe.
    ext2_file_type_socket,           ///< Socket
    ext2_file_type_symbolic_link     ///< Symbolic link.
} ext2_file_type_t;

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
    /// Number of the inode that this directory entry points to.
    uint32_t inode;
    /// Length of this directory entry. Must be a multiple of 4.
    uint16_t rec_len;
    /// Length of the file name.
    uint8_t name_len;
    /// File type code.
    uint8_t file_type;
    /// File name.
    char name[];
} ext2_dirent_t;

/// @brief The details regarding the filesystem.
typedef struct ext2_filesystem_t {
    /// Pointer to the block device.
    vfs_file_t *block_device;
    /// Device superblock, contains important information.
    ext2_superblock_t superblock;
    /// Block Group Descriptor / Block groups.
    ext2_group_descriptor_t *block_groups;
    /// EXT2 memory cache for buffers.
    kmem_cache_t *ext2_buffer_cache;
    /// Root FS node (attached to mountpoint).
    vfs_file_t *root;
    /// List of opened files.
    list_head opened_files;

    /// Size of one block.
    uint32_t block_size;

    /// Number of inodes that fit in a block.
    uint32_t inodes_per_block_count;
    /// Number of blocks that fit in a block.
    uint32_t blocks_per_block_count;
    /// Number of blocks groups.
    uint32_t block_groups_count;
    /// Number of block pointers per block.
    uint32_t pointers_per_block;
    /// Index in terms of blocks where the BGDT starts.
    uint32_t bgdt_start_block;
    /// Index in terms of blocks where the BGDT ends.
    uint32_t bgdt_end_block;
    /// The number of blocks containing the BGDT
    uint32_t bgdt_length;

    /// Index of indirect blocks.
    uint32_t indirect_blocks_index;
    /// Index of doubly-indirect blocks.
    uint32_t doubly_indirect_blocks_index;
    /// Index of trebly-indirect blocks.
    uint32_t trebly_indirect_blocks_index;

    /// Spinlock for protecting filesystem operations.
    spinlock_t spinlock;
} ext2_filesystem_t;

/// @brief Structure used when searching for a directory entry.
typedef struct ext2_direntry_search_t {
    /// Pointer to the direntry where we store the search results.
    ext2_dirent_t *direntry;
    /// The inode of the parent directory.
    ino_t parent_inode;
    /// The index of the block where the direntry resides.
    uint32_t block_index;
    /// The offest of the direntry inside the block.
    uint32_t block_offset;
} ext2_direntry_search_t;

// ============================================================================
// Forward Declaration of Functions
// ============================================================================

static ext2_block_status_t ext2_check_bitmap_bit(uint8_t *buffer, uint32_t index);
static void ext2_set_bitmap_bit(uint8_t *buffer, uint32_t index, ext2_block_status_t status);

static int ext2_read_superblock(ext2_filesystem_t *fs);
static int ext2_write_superblock(ext2_filesystem_t *fs);
static int ext2_read_block(ext2_filesystem_t *fs, uint32_t block_index, uint8_t *buffer);
static int ext2_write_block(ext2_filesystem_t *fs, uint32_t block_index, uint8_t *buffer);
static int ext2_read_bgdt(ext2_filesystem_t *fs);
static int ext2_write_bgdt(ext2_filesystem_t *fs);
static int ext2_read_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index);
static int ext2_write_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index);

static vfs_file_t *ext2_open(const char *path, int flags, mode_t mode);
static int ext2_unlink(const char *path);
static int ext2_close(vfs_file_t *file);
static ssize_t ext2_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte);
static ssize_t ext2_write(vfs_file_t *file, const void *buffer, off_t offset, size_t nbyte);
static off_t ext2_lseek(vfs_file_t *file, off_t offset, int whence);
static int ext2_fstat(vfs_file_t *file, stat_t *stat);
static int ext2_ioctl(vfs_file_t *file, int request, void *data);
static int ext2_getdents(vfs_file_t *file, dirent_t *dirp, off_t doff, size_t count);

static int ext2_mkdir(const char *path, mode_t mode);
static int ext2_rmdir(const char *path);
static int ext2_stat(const char *path, stat_t *stat);

static vfs_file_t *ext2_mount(vfs_file_t *block_device, const char *path);

// ============================================================================
// Virtual FileSystem (VFS) Operaions
// ============================================================================

/// Filesystem general operations.
static vfs_sys_operations_t ext2_sys_operations = {
    .mkdir_f = ext2_mkdir,
    .rmdir_f = ext2_rmdir,
    .stat_f  = ext2_stat
};

/// Filesystem file operations.
static vfs_file_operations_t ext2_fs_operations = {
    .open_f     = ext2_open,
    .unlink_f   = ext2_unlink,
    .close_f    = ext2_close,
    .read_f     = ext2_read,
    .write_f    = ext2_write,
    .lseek_f    = ext2_lseek,
    .stat_f     = ext2_fstat,
    .ioctl_f    = ext2_ioctl,
    .getdents_f = ext2_getdents
};

// ============================================================================
// Debugging Support Functions
// ============================================================================

/// @brief Turns an UUID to string.
/// @param uuid the UUID to turn to string.
/// @return the string representing the UUID.
static const char *uuid_to_string(uint8_t uuid[16])
{
    static char s[33] = { 0 };
    sprintf(s, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
            uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
    return s;
}

static int ext2_file_type_to_vfs_file_type(int ext2_type)
{
    if (ext2_type == ext2_file_type_regular_file)
        return DT_REG;
    if (ext2_type == ext2_file_type_directory)
        return DT_DIR;
    if (ext2_type == ext2_file_type_character_device)
        return DT_CHR;
    if (ext2_type == ext2_file_type_block_device)
        return DT_BLK;
    if (ext2_type == ext2_file_type_named_pipe)
        return DT_FIFO;
    if (ext2_type == ext2_file_type_socket)
        return DT_SOCK;
    if (ext2_type == ext2_file_type_symbolic_link)
        return DT_LNK;
    return DT_UNKNOWN;
}

/// @brief Turns the time to string.
/// @param time the UNIX time to turn to string.
/// @return time turned to string.
static const char *time_to_string(uint32_t time)
{
    static char s[250] = { 0 };
    tm_t *tm           = localtime(&time);
    sprintf(s, "%2d/%2d %2d:%2d", tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min);
    return s;
}

/// @brief Dumps on debugging output the superblock.
/// @param sb the object to dump.
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

/// @brief Dumps on debugging output the group descriptor.
/// @param gd the object to dump.
static void ext2_dump_group_descriptor(ext2_group_descriptor_t *gd)
{
    pr_debug("block_bitmap          : %d\n", gd->block_bitmap);
    pr_debug("inode_bitmap          : %d\n", gd->inode_bitmap);
    pr_debug("inode_table           : %d\n", gd->inode_table);
    pr_debug("free_blocks_count     : %d\n", gd->free_blocks_count);
    pr_debug("free_inodes_count     : %d\n", gd->free_inodes_count);
    pr_debug("used_dirs_count       : %d\n", gd->used_dirs_count);
}

/// @brief Dumps on debugging output the inode.
/// @param inode the object to dump.
static void ext2_dump_inode(ext2_inode_t *inode)
{
    pr_debug("mode          : %u\n", inode->mode);
    pr_debug("uid           : %u\n", inode->uid);
    pr_debug("size          : %u\n", inode->size);
    pr_debug("atime         : %u\n", inode->atime);
    pr_debug("ctime         : %u\n", inode->ctime);
    pr_debug("mtime         : %u\n", inode->mtime);
    pr_debug("dtime         : %u\n", inode->dtime);
    pr_debug("gid           : %u\n", inode->gid);
    pr_debug("links_count   : %u\n", inode->links_count);
    pr_debug("blocks_count  : %u\n", inode->blocks_count);
    pr_debug("flags         : %u\n", inode->flags);
    pr_debug("osd1          : %u\n", inode->osd1);
    pr_debug("data          : {\n");
    for (int i = 0; i < EXT2_INDIRECT_BLOCKS; ++i)
        pr_debug("    data.blocks.D[%2d] : %u\n", i, inode->data.blocks.dir_blocks[i]);
    pr_debug("    data.blocks.IND_B  : %u\n", inode->data.blocks.indir_block);
    pr_debug("    data.blocks.DBL_B  : %u\n", inode->data.blocks.doubly_indir_block);
    pr_debug("    data.blocks.TRB_B  : %u\n", inode->data.blocks.trebly_indir_block);
    pr_debug("    data.symblink      : %s\n", inode->data.symlink);
    pr_debug("data          : }\n");
    pr_debug("generation    : %u\n", inode->generation);
    pr_debug("file_acl      : %u\n", inode->file_acl);
    pr_debug("dir_acl       : %u\n", inode->dir_acl);
    pr_debug("osd2[0]       : %u\n", inode->osd2[0]);
    pr_debug("osd2[1]       : %u\n", inode->osd2[1]);
    pr_debug("osd2[2]       : %u\n", inode->osd2[2]);
}

/// @brief Dumps on debugging output the BGDT.
/// @param fs the filesystem of which we print the BGDT.
static void ext2_dump_bgdt(ext2_filesystem_t *fs)
{
    // Allocate the cache.
    uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    // Clean the cache.
    memset(cache, 0, fs->block_size);
    for (uint32_t i = 0; i < fs->block_groups_count; ++i) {
        // Get the pointer to the current group descriptor.
        ext2_group_descriptor_t *gd = &(fs->block_groups[i]);
        pr_debug("Block Group Descriptor [%d] @ %d:\n", i, fs->bgdt_start_block + i * fs->superblock.blocks_per_group);
        pr_debug("    Free Blocks : %4d of %d\n", gd->free_blocks_count, fs->superblock.blocks_per_group);
        pr_debug("    Free Inodes : %4d of %d\n", gd->free_inodes_count, fs->superblock.inodes_per_group);
        // Dump the block bitmap.
        ext2_read_block(fs, gd->block_bitmap, cache);
        pr_debug("    Block Bitmap at %d\n", gd->block_bitmap);
        for (uint32_t j = 0; j < fs->block_size; ++j) {
            if ((j % 8) == 0)
                pr_debug("        Block index: %4d, Bitmap: %s\n", j / 8, dec_to_binary(cache[j / 8], 8));
            if (!ext2_check_bitmap_bit(cache, j)) {
                pr_debug("    First free block in group is in block %d, the linear index is %d\n", j / 8, j);
                break;
            }
        }
        // Dump the block bitmap.
        ext2_read_block(fs, gd->inode_bitmap, cache);
        pr_debug("    Inode Bitmap at %d\n", gd->inode_bitmap);
        for (uint32_t j = 0; j < fs->block_size; ++j) {
            if ((j % 8) == 0)
                pr_debug("        Block index: %4d, Bitmap: %s\n", j / 8, dec_to_binary(cache[j / 8], 8));
            if (!ext2_check_bitmap_bit(cache, j)) {
                pr_debug("    First free block in group is in block %d, the linear index is %d\n", j / 8, j);
                break;
            }
        }
    }
    kmem_cache_free(cache);
}

/// @brief Dumps on debugging output the filesystem.
/// @param fs the object to dump.
static void ext2_dump_filesystem(ext2_filesystem_t *fs)
{
    pr_debug("block_device          : 0x%x\n", fs->block_device);
    pr_debug("superblock            : 0x%x\n", fs->superblock);
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

// ============================================================================
// EXT2 Core Functions
// ============================================================================

/// @brief Determining which block group contains an inode.
/// @param fs the ext2 filesystem structure.
/// @param inode_index the inode index.
/// @return the group index.
/// @details Remember that inode addressing starts from 1.
static uint32_t ext2_get_group_index_from_inode(ext2_filesystem_t *fs, uint32_t inode_index)
{
    return (inode_index - 1) / fs->superblock.inodes_per_group;
}

/// @brief Determining the offest of the inode inside the block group.
/// @param fs the ext2 filesystem structure.
/// @param inode_index the inode index.
/// @return the offset of the inode inside the group.
/// @details Remember that inode addressing starts from 1.
static uint32_t ext2_get_inode_offest_in_group(ext2_filesystem_t *fs, uint32_t inode_index)
{
    return (inode_index - 1) % fs->superblock.inodes_per_group;
}

/// @brief Determines which block contains our inode.
/// @param fs the ext2 filesystem structure.
/// @param inode_offset the inode offset inside the group.
/// @return which block contains our inode.
static uint32_t ext2_get_block_index_from_inode_offset(ext2_filesystem_t *fs, uint32_t inode_offset)
{
    return (inode_offset * fs->superblock.inode_size) / fs->block_size;
}

/// @brief Cheks if the bit at the given linear index is free.
/// @param buffer the buffer containing the bitmap
/// @param linear_index the linear index we want to check.
/// @return if the bit is 0 or 1.
/// @details
/// How we access the specific bits inside the bitmap takes inspiration from the
/// mailman's algorithm.
static ext2_block_status_t ext2_check_bitmap_bit(uint8_t *buffer, uint32_t linear_index)
{
    return (ext2_block_status_t)(bit_check(buffer[linear_index / 8], linear_index % 8) != 0);
}

/// @brief Sets the bit at the given linear index accordingly to `status`.
/// @param buffer the buffer containing the bitmap
/// @param linear_index the linear index we want to check.
/// @param status the new status of the block (free|occupied).
static void ext2_set_bitmap_bit(uint8_t *buffer, uint32_t linear_index, ext2_block_status_t status)
{
    if (status == ext2_block_status_occupied)
        bit_set_assign(buffer[linear_index / 8], linear_index % 8);
    else
        bit_clear_assign(buffer[linear_index / 8], linear_index % 8);
}

/// @brief Searches for a free inode inside the group data loaded inside the cache.
/// @param fs the ext2 filesystem structure.
/// @param cache the cache from which we read the bgdt data.
/// @param linear_index the output variable where we store the linear indes to the free inode.
/// @return true if we found a free inode, false otherwise.
static inline bool_t ext2_find_free_inode_in_group(
    ext2_filesystem_t *fs,
    uint8_t *cache,
    uint32_t *linear_index,
    bool_t skip_reserved)
{
    for ((*linear_index) = 0; (*linear_index) < fs->superblock.inodes_per_group; ++(*linear_index)) {
        // If we need to skip the reserved inodes, we skip the round if the
        // index is that of a reserved inode (superblock.first_ino).
        if (skip_reserved && ((*linear_index) < fs->superblock.first_ino))
            continue;
        // Check if the entry is free.
        if (!ext2_check_bitmap_bit(cache, *linear_index))
            return true;
    }
    return false;
}

/// @brief Searches for a free inode inside the Block Group Descriptor Table (BGDT).
/// @param fs the ext2 filesystem structure.
/// @param cache the cache from which we read the bgdt data.
/// @param group_index the output variable where we store the group index.
/// @param linear_index the output variable where we store the linear indes to the free inode.
/// @return true if we found a free inode, false otherwise.
static inline bool_t ext2_find_free_inode(
    ext2_filesystem_t *fs,
    uint8_t *cache,
    uint32_t *group_index,
    uint32_t *linear_index,
    uint32_t preferred_group)
{
    // If we received a preference, try to find a free inode in that specific group.
    if (preferred_group != 0) {
        // Set the group index to the preferred group.
        (*group_index) = preferred_group;
        // Find the first free inode. We need to ask to skip reserved inodes,
        // only if we are in group 0.
        if (ext2_find_free_inode_in_group(fs, cache, linear_index, (*group_index) == 0))
            return true;
    }
    // Get the group and bit index of the first free block.
    for ((*group_index) = 0; (*group_index) < fs->block_groups_count; ++(*group_index)) {
        // Check if there are free inodes in this block group.
        if (fs->block_groups[(*group_index)].free_inodes_count > 0) {
            // Read the block bitmap.
            if (ext2_read_block(fs, fs->block_groups[(*group_index)].inode_bitmap, cache) < 0) {
                pr_err("Failed to read the inode bitmap for group `%d`.\n", (*group_index));
                return false;
            }
            // Find the first free inode. We need to ask to skip reserved
            // inodes, only if we are in group 0.
            if (ext2_find_free_inode_in_group(fs, cache, linear_index, (*group_index) == 0))
                return true;
        }
    }
    return false;
}

/// @brief Searches for a free block inside the group data loaded inside the cache.
/// @param fs the ext2 filesystem structure.
/// @param cache the cache from which we read the bgdt data.
/// @param group_index the output variable where we store the group index.
/// @param linear_index the output variable where we store the linear indes to the free block.
/// @return true if we found a free block, false otherwise.
static inline bool_t ext2_find_free_block_in_group(ext2_filesystem_t *fs, uint8_t *cache, uint32_t *linear_index)
{
    for ((*linear_index) = 0; (*linear_index) < fs->superblock.blocks_per_group; ++(*linear_index)) {
        // Check if the entry is free.
        if (!ext2_check_bitmap_bit(cache, *linear_index))
            return true;
    }
    return false;
}

/// @brief Searches for a free block.
/// @param fs the ext2 filesystem structure.
/// @param cache the cache from which we read the bgdt data.
/// @param linear_index the output variable where we store the linear indes to the free block.
/// @return true if we found a free block, false otherwise.
static inline bool_t ext2_find_free_block(
    ext2_filesystem_t *fs,
    uint8_t *cache,
    uint32_t *group_index,
    uint32_t *linear_index)
{
    // Get the group and bit index of the first free block.
    for ((*group_index) = 0; (*group_index) < fs->block_groups_count; ++(*group_index)) {
        // Check if there are free blocks in this block group.
        if (fs->block_groups[(*group_index)].free_blocks_count > 0) {
            // Read the block bitmap.
            if (ext2_read_block(fs, fs->block_groups[(*group_index)].block_bitmap, cache) < 0) {
                pr_err("Failed to read the block bitmap for group `%d`.\n", (*group_index));
                return false;
            }
            // Find the first free block.
            if (ext2_find_free_block_in_group(fs, cache, linear_index))
                return true;
        }
    }
    return false;
}

/// @brief Reads the superblock from the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return the amount of data we read, or negative value for an error.
static int ext2_read_superblock(ext2_filesystem_t *fs)
{
    pr_debug("Read superblock for EXT2 filesystem (0x%x)\n", fs);
    return vfs_read(fs->block_device, &fs->superblock, 1024, sizeof(ext2_superblock_t));
}

/// @brief Writes the superblock on the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return the amount of data we wrote, or negative value for an error.
static int ext2_write_superblock(ext2_filesystem_t *fs)
{
    pr_debug("Write superblock for EXT2 filesystem (0x%x)\n", fs);
    return vfs_write(fs->block_device, &fs->superblock, 1024, sizeof(ext2_superblock_t));
}

/// @brief Read a block from the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @param block_index the index of the block we want to read.
/// @param buffer the buffer where the content will be placed.
/// @return the amount of data we read, or negative value for an error.
static int ext2_read_block(ext2_filesystem_t *fs, uint32_t block_index, uint8_t *buffer)
{
    //pr_debug("Read block %4d for EXT2 filesystem (0x%x)\n", block_index, fs);
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
static int ext2_write_block(ext2_filesystem_t *fs, uint32_t block_index, uint8_t *buffer)
{
    //pr_debug("Write block %4d for EXT2 filesystem (0x%x)\n", block_index, fs);
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
static int ext2_read_bgdt(ext2_filesystem_t *fs)
{
    pr_debug("Read BGDT for EXT2 filesystem (0x%x)\n", fs);
    if (fs->block_groups) {
        for (uint32_t i = 0; i < fs->bgdt_length; ++i)
            ext2_read_block(fs, fs->bgdt_start_block + i, (uint8_t *)((uintptr_t)fs->block_groups + (fs->block_size * i)));
        return 0;
    }
    pr_err("The `block_groups` list is not initialized.\n");
    return -1;
}

/// @brief Writes the Block Group Descriptor Table (BGDT) to the block device associated with this filesystem.
/// @param fs the ext2 filesystem structure.
/// @return 0 on success, -1 on failure.
static int ext2_write_bgdt(ext2_filesystem_t *fs)
{
    pr_debug("Write BGDT for EXT2 filesystem (0x%x)\n", fs);
    if (fs->block_groups) {
        for (uint32_t i = 0; i < fs->bgdt_length; ++i)
            ext2_write_block(fs, fs->bgdt_start_block + i, (uint8_t *)((uintptr_t)fs->block_groups + (fs->block_size * i)));
        return 0;
    }
    pr_err("The `block_groups` list is not initialized.\n");
    return -1;
}

/// @brief Reads an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @return 0 on success, -1 on failure.
static int ext2_read_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index)
{
    if (inode_index == 0) {
        pr_err("You are trying to read an invalid inode index (%d).\n", inode_index);
        return -1;
    }
    // Retrieve the group index.
    uint32_t group_index = ext2_get_group_index_from_inode(fs, inode_index);
    if (group_index > fs->block_groups_count) {
        pr_err("Invalid group index computed from inode index `%d`.\n", inode_index);
        return -1;
    }
    // Get the index of the inode inside the group.
    uint32_t offset = ext2_get_inode_offest_in_group(fs, inode_index);
    // Get the block offest.
    uint32_t block = ext2_get_block_index_from_inode_offset(fs, offset);
    // Get the real inode offset inside the block.
    offset %= fs->inodes_per_block_count;
    // Allocate the cache.
    uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    // Clean the cache.
    memset(cache, 0, fs->block_size);
    // Read the block containing the inode table.
    ext2_read_block(fs, fs->block_groups[group_index].inode_table + block, cache);
    // Save the inode content.
    memcpy(inode, (ext2_inode_t *)((uintptr_t)cache + (offset * fs->superblock.inode_size)), fs->superblock.inode_size);
    // Free the cache.
    kmem_cache_free(cache);
    return 0;
}

/// @brief Writes the inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @return 0 on success, -1 on failure.
static int ext2_write_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index)
{
    if (inode_index == 0) {
        pr_err("You are trying to read an invalid inode index (%d).\n", inode_index);
        return -1;
    }
    // Retrieve the group index.
    uint32_t group_index = ext2_get_group_index_from_inode(fs, inode_index);
    if (group_index > fs->block_groups_count) {
        pr_err("Invalid group index computed from inode index `%d`.\n", inode_index);
        return -1;
    }
    // Get the offset of the inode inside the group.
    uint32_t offset = ext2_get_inode_offest_in_group(fs, inode_index);
    // Get the block offest.
    uint32_t block = ext2_get_block_index_from_inode_offset(fs, offset);
    // Get the real inode offset inside the block.
    offset %= fs->inodes_per_block_count;
    // Allocate the cache.
    uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    // Clean the cache.
    memset(cache, 0, fs->block_size);
    // Read the block containing the inode table.
    ext2_read_block(fs, fs->block_groups[group_index].inode_table + block, cache);
    // Write the inode.
    memcpy((ext2_inode_t *)((uintptr_t)cache + (offset * fs->superblock.inode_size)), inode, fs->superblock.inode_size);
    // Write back the block.
    ext2_write_block(fs, fs->block_groups[group_index].inode_table + block, cache);
    // Free the cache.
    kmem_cache_free(cache);
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
static int ext2_allocate_inode(ext2_filesystem_t *fs, unsigned preferred_group)
{
    uint32_t group_index = 0, linear_index = 0, inode_index = 0;
    // Lock the filesystem.
    spinlock_lock(&fs->spinlock);
    // Allocate the cache.
    uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    // Clean the cache.
    memset(cache, 0, fs->block_size);
    // Search for a free inode.
    if (!ext2_find_free_inode(fs, cache, &group_index, &linear_index, preferred_group)) {
        pr_warning("Failed to find a free inode.\n");
        // Unlock the filesystem.
        spinlock_unlock(&fs->spinlock);
        // Free the cache.
        kmem_cache_free(cache);
        return 0;
    }
    // Compute the inode index.
    inode_index = (group_index * fs->superblock.inodes_per_group) + linear_index + 1U;
    // Set the inode as occupied.
    ext2_set_bitmap_bit(cache, linear_index, ext2_block_status_occupied);
    // Write back the inode bitmap.
    ext2_write_block(fs, fs->block_groups[group_index].inode_bitmap, cache);
    // Free the cache.
    kmem_cache_free(cache);
    // Reduce the number of free inodes.
    fs->block_groups[group_index].free_inodes_count -= 1;
    // Update the bgdt.
    ext2_write_bgdt(fs);
    // Reduce the number of inodes inside the superblock.
    fs->superblock.free_inodes_count -= 1;
    // Update the superblock.
    ext2_write_superblock(fs);
    // Unlock the filesystem.
    spinlock_unlock(&fs->spinlock);
    // Return the inode.
    return inode_index;
}

/// @brief Allocates a new block.
/// @param fs the filesystem.
/// @return 0 on failure, or the index of the new block on success.
static uint32_t ext2_allocate_block(ext2_filesystem_t *fs)
{
    uint32_t group_index = 0, linear_index = 0, block_index = 0;
    // Lock the filesystem.
    spinlock_lock(&fs->spinlock);
    // Allocate the cache.
    uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    // Clean the cache.
    memset(cache, 0, fs->block_size);
    // Search for a free block.
    if (!ext2_find_free_block(fs, cache, &group_index, &linear_index)) {
        pr_warning("Failed to find a free block.\n");
        // Unlock the filesystem.
        spinlock_unlock(&fs->spinlock);
        // Free the cache.
        kmem_cache_free(cache);
        return 0;
    }
    // Compute the block index.
    block_index = (group_index * fs->superblock.blocks_per_group) + linear_index;
    // Set the block as occupied.
    ext2_set_bitmap_bit(cache, linear_index, ext2_block_status_occupied);
    // Update the bitmap.
    ext2_write_block(fs, fs->block_groups[group_index].block_bitmap, cache);
    // Decrease the number of free blocks inside the BGDT entry.
    fs->block_groups[group_index].free_blocks_count -= 1;
    // Update the BGDT.
    ext2_write_bgdt(fs);
    // Decrease the number of free blocks inside the superblock.
    fs->superblock.free_blocks_count -= 1;
    // Update the superblock.
    ext2_write_superblock(fs);
    // Empty out the new block.
    memset(cache, 0, fs->block_size);
    ext2_write_block(fs, block_index, cache);
    // Free the cache.
    kmem_cache_free(cache);
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
            uint32_t new_block_index = ext2_allocate_block(fs);
            if (new_block_index == 0)
                return -1;
            // Update the index.
            inode->data.blocks.indir_block = new_block_index;
            // Update the inode.
            if (ext2_write_inode(fs, inode, inode_index) == -1)
                return -1;
        }

        // Allocate the cache.
        uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
        // Clean the cache.
        memset(cache, 0, fs->block_size);
        // Read the indirect block (which contains pointers to the next set of blocks).
        ext2_read_block(fs, inode->data.blocks.indir_block, cache);
        // Write the index inside the final block.
        ((uint32_t *)cache)[a] = real_index;
        // Write back the indirect block.
        ext2_read_block(fs, inode->data.blocks.indir_block, cache);
        // Free the cache.
        kmem_cache_free(cache);
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
            uint32_t new_block_index = ext2_allocate_block(fs);
            if (new_block_index == 0)
                return -1;
            // Update the index.
            inode->data.blocks.doubly_indir_block = new_block_index;
            // Update the inode.
            if (ext2_write_inode(fs, inode, inode_index) == -1)
                return -1;
        }

        // Allocate the cache.
        uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
        // Clean the cache.
        memset(cache, 0, fs->block_size);

        // Read the doubly-indirect block (which contains pointers to indirect blocks).
        ext2_read_block(fs, inode->data.blocks.doubly_indir_block, cache);
        // Check that the indirect block points to a valid block.
        if (!((uint32_t *)cache)[c]) {
            // Allocate a new block.
            uint32_t new_block_index = ext2_allocate_block(fs);
            if (new_block_index == 0) {
                // Free the cache.
                kmem_cache_free(cache);
                return -1;
            }
            // Update the index.
            ((uint32_t *)cache)[c] = new_block_index;
            // Write the doubly-indirect block back.
            ext2_write_block(fs, inode->data.blocks.doubly_indir_block, cache);
        }

        // Compute the index inside the indirect block.
        ext2_read_block(fs, ((uint32_t *)cache)[c], cache);
        // Write the index inside the final block.
        ((uint32_t *)cache)[d] = real_index;
        // Write back the indirect block.
        ext2_read_block(fs, ((uint32_t *)cache)[c], cache);
        // Free the cache.
        kmem_cache_free(cache);
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
            uint32_t new_block_index = ext2_allocate_block(fs);
            if (new_block_index == 0)
                return -1;
            // Update the index.
            inode->data.blocks.trebly_indir_block = new_block_index;
            // Update the inode.
            if (ext2_write_inode(fs, inode, inode_index) == -1)
                return -1;
        }

        // Allocate the cache.
        uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
        // Clean the cache.
        memset(cache, 0, fs->block_size);

        // Read the trebly-indirect block (which contains pointers to doubly-indirect blocks).
        ext2_read_block(fs, inode->data.blocks.trebly_indir_block, cache);
        // Check that the indirect block points to a valid block.
        if (!((uint32_t *)cache)[d]) {
            // Allocate a new block.
            uint32_t new_block_index = ext2_allocate_block(fs);
            if (new_block_index == 0) {
                // Free the cache.
                kmem_cache_free(cache);
                return -1;
            }
            // Update the index.
            ((uint32_t *)cache)[d] = new_block_index;
            // Write the doubly-indirect block back.
            ext2_write_block(fs, inode->data.blocks.trebly_indir_block, cache);
        }
        // Save the block index, otherwise with the next read we lose the block index list [d].
        block_index_save = ((uint32_t *)cache)[d];

        // Read the doubly-indirect block (which contains pointers to indirect blocks).
        ext2_read_block(fs, block_index_save, cache);
        // Check that the indirect block points to a valid block.
        if (!((uint32_t *)cache)[f]) {
            // Allocate a new block.
            uint32_t new_block_index = ext2_allocate_block(fs);
            if (new_block_index == 0) {
                // Free the cache.
                kmem_cache_free(cache);
                return -1;
            }
            // Update the index.
            ((uint32_t *)cache)[f] = new_block_index;
            // Write the doubly-indirect block back.
            ext2_write_block(fs, block_index_save, cache);
        }

        // Get the next group index.
        block_index_save = ((uint32_t *)cache)[f];
        // Read the indirect block (which contains pointers to the next set of blocks).
        ext2_read_block(fs, block_index_save, cache);
        // Write the index inside the final block.
        ((uint32_t *)cache)[g] = real_index;
        // Write back the indirect block.
        ext2_read_block(fs, block_index_save, cache);
        // Free the cache.
        kmem_cache_free(cache);
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
    // Create a variable for the real index.
    uint32_t real_index = 0;
    // For simplicity.
    uint32_t p1 = fs->pointers_per_block;
    uint32_t p2 = fs->pointers_per_block * fs->pointers_per_block;
    // Allocate the cache.
    uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    // Clean the cache.
    memset(cache, 0, fs->block_size);
    // Check if the index is among the indirect blocks.
    if (block_index < fs->indirect_blocks_index) {
        // Compute the indirect indices.
        uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;
        // Read the indirect block (which contains pointers to the next set of blocks).
        ext2_read_block(fs, inode->data.blocks.indir_block, cache);
        // Compute the index inside the final block.
        real_index = ((uint32_t *)cache)[a];
    } else if (block_index < fs->doubly_indirect_blocks_index) {
        // The index is among the doubly-indirect blocks.
        // Compute the indirect indices.
        uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;
        uint32_t b = a - p1;
        uint32_t c = b / p1;
        uint32_t d = b - (c * p1);
        // Read the doubly-indirect block (which contains pointers to indirect blocks).
        ext2_read_block(fs, inode->data.blocks.doubly_indir_block, cache);
        // Compute the index inside the indirect block.
        ext2_read_block(fs, ((uint32_t *)cache)[c], cache);
        // Compute the index inside the final block.
        real_index = ((uint32_t *)cache)[d];
    } else if (block_index < fs->trebly_indirect_blocks_index) {
        // The index is among the trebly-indirect blocks.
        // Compute the indirect indices.
        uint32_t a = block_index - EXT2_INDIRECT_BLOCKS;
        uint32_t b = a - p1;
        uint32_t c = b - p2;
        uint32_t d = c / p2;
        uint32_t e = c - (d * p2);
        uint32_t f = e / p1;
        uint32_t g = e - (f * p1);
        // Read the trebly-indirect block (which contains pointers to doubly-indirect blocks).
        ext2_read_block(fs, inode->data.blocks.trebly_indir_block, cache);
        // Read the doubly-indirect block (which contains pointers to indirect blocks).
        ext2_read_block(fs, ((uint32_t *)cache)[d], cache);
        // Read the indirect block (which contains pointers to the next set of blocks).
        ext2_read_block(fs, ((uint32_t *)cache)[f], cache);
        // Compute the index inside the final block.
        real_index = ((uint32_t *)cache)[g];
    } else {
        pr_err("We failed to retrieve the real block number of the block with index `%d`\n", block_index);
    }
    // Free the cache.
    kmem_cache_free(cache);
    // Return the real index.
    return real_index;
}

/// @brief Allocate a new block for an inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @param block_index The index of the block within the inode.
/// @return 0 on success, -1 on failure.
static int ext2_allocate_inode_block(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index, uint32_t block_index)
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
    uint32_t blocks_count = (block_index + 1) * fs->blocks_per_block_count;
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
/// @return the amount of data we read, or negative value for an error.
static ssize_t ext2_read_inode_block(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t block_index, uint8_t *buffer)
{
    if (block_index >= (inode->blocks_count / fs->blocks_per_block_count)) {
        pr_err("Tried to read an invalid block `%d`, but inode only has %d\n",
               block_index, (inode->blocks_count / fs->blocks_per_block_count));
        return -1;
    }
    // Get the real index.
    uint32_t real_index = ext2_get_real_block_index(fs, inode, block_index);
    if (real_index == 0)
        return -1;
    // Read the block.
    return ext2_read_block(fs, real_index, buffer);
}

/// @brief Writes the real block starting from an inode and the block index inside the inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index The index of the inode.
/// @param block_index the index of the block within the inode.
/// @param buffer the buffer where to put the data.
/// @return the amount of data we wrote, or negative value for an error.
static ssize_t ext2_write_inode_block(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index, uint32_t block_index, uint8_t *buffer)
{
    while (block_index >= (inode->blocks_count / fs->blocks_per_block_count)) {
        pr_err("Tried to read an invalid block `%d`, but inode only has %d!",
               block_index, (inode->blocks_count / fs->blocks_per_block_count));
        ext2_allocate_inode_block(fs, inode, inode_index, (inode->blocks_count / fs->blocks_per_block_count));
        ext2_write_inode(fs, inode, inode_index);
    }
    // Get the real index.
    uint32_t real_index = ext2_get_real_block_index(fs, inode, block_index);
    if (real_index == 0)
        return -1;
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
static ssize_t ext2_read_inode_data(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index, off_t offset, size_t nbyte, char *buffer)
{
    // Check if the file is empty.
    if (inode->size == 0)
        return 0;

    uint32_t end;
    if ((offset + nbyte) > inode->size) {
        end = inode->size;
    } else {
        end = offset + nbyte;
    }
    uint32_t start_block  = offset / fs->block_size;
    uint32_t end_block    = end / fs->block_size;
    uint32_t end_size     = end - end_block * fs->block_size;
    uint32_t size_to_read = end - offset;

    // Allocate the cache.
    uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    // Clean the cache.
    memset(cache, 0, fs->block_size);

    if (start_block == end_block) {
        // Read the real block.
        if (ext2_read_inode_block(fs, inode, start_block, cache) == -1) {
            pr_err("Failed to read the inode block `%d`\n", start_block);
            goto free_cache_return_error;
        }
        // Copy the content back to the buffer.
        memcpy(buffer, (uint8_t *)(((uintptr_t)cache) + ((uintptr_t)offset % fs->block_size)), size_to_read);
    } else {
        uint32_t block_offset;
        uint32_t blocks_read = 0;
        for (block_offset = start_block; block_offset < end_block; block_offset++, blocks_read++) {
            // Read the real block.
            if (ext2_read_inode_block(fs, inode, block_offset, cache) == -1) {
                pr_err("Failed to read the inode block `%d`\n", block_offset);
                goto free_cache_return_error;
            }
            // Copy the content back to the buffer.
            if (block_offset == start_block) {
                memcpy(buffer, (uint8_t *)(((uintptr_t)cache) + ((uintptr_t)offset % fs->block_size)), fs->block_size - (offset % fs->block_size));
            } else {
                memcpy(buffer + fs->block_size * blocks_read - (offset % fs->block_size), cache, fs->block_size);
            }
        }
        if (end_size) {
            // Read the real block.
            if (ext2_read_inode_block(fs, inode, end_block, cache) == -1) {
                pr_err("Failed to read the inode block `%d`\n", block_offset);
                goto free_cache_return_error;
            }
            // Copy the content back to the buffer.
            memcpy(buffer + fs->block_size * blocks_read - (offset % fs->block_size), cache, end_size);
        }
    }
    // Free the cache.
    kmem_cache_free(cache);
    return size_to_read;
free_cache_return_error:
    // Free the cache.
    kmem_cache_free(cache);
    return -1;
}

/// @brief Writes the data on the given inode.
/// @param fs the filesystem.
/// @param inode the inode which we are working with.
/// @param inode_index the index of the inode.
/// @param offset the offset from which we start writing the data.
/// @param nbyte the number of bytes to write.
/// @param buffer the buffer containing the data.
/// @return the amount written.
static ssize_t ext2_write_inode_data(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index, off_t offset, size_t nbyte, char *buffer)
{
    uint32_t end = offset + nbyte;
    if (end > inode->size) {
        inode->size = end;
    }
    uint32_t start_block   = offset / fs->block_size;
    uint32_t end_block     = end / fs->block_size;
    uint32_t end_size      = end - end_block * fs->block_size;
    uint32_t size_to_write = end - offset;

    // Allocate the cache.
    uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    // Clean the cache.
    memset(cache, 0, fs->block_size);

    if (start_block == end_block) {
        // Read the real block.
        if (ext2_read_inode_block(fs, inode, start_block, cache) == -1) {
            pr_err("Failed to read the inode block `%d`\n", start_block);
            goto free_cache_return_error;
        }
        // Copy the content back to the buffer.
        memcpy((uint8_t *)(((uintptr_t)cache) + ((uintptr_t)offset % fs->block_size)), buffer, size_to_write);
        // Write the block back.
        if (!ext2_write_inode_block(fs, inode, inode_index, start_block, cache)) {
            pr_err("Failed to write the inode block `%d`\n", start_block);
            goto free_cache_return_error;
        }
    } else {
        uint32_t block_offset, blocks_read = 0;
        for (block_offset = start_block; block_offset < end_block; ++block_offset, ++blocks_read) {
            // Read the real block.
            if (ext2_read_inode_block(fs, inode, block_offset, cache) == -1) {
                pr_err("Failed to read the inode block `%d`\n", block_offset);
                goto free_cache_return_error;
            }
            if (block_offset == start_block) {
                // Copy the content back to the buffer.
                memcpy((uint8_t *)(((uintptr_t)cache) + ((uintptr_t)offset % fs->block_size)), buffer, fs->block_size - (offset % fs->block_size));
            } else {
                // Copy the content back to the buffer.
                memcpy(cache, buffer + fs->block_size * blocks_read - (offset % fs->block_size), fs->block_size);
            }
            // Write the block back.
            if (!ext2_write_inode_block(fs, inode, inode_index, block_offset, cache)) {
                pr_err("Failed to write the inode block `%d`\n", start_block);
                goto free_cache_return_error;
            }
        }
        if (end_size) {
            // Read the real block.
            if (ext2_read_inode_block(fs, inode, end_block, cache) == -1) {
                pr_err("Failed to read the inode block `%d`\n", block_offset);
                goto free_cache_return_error;
            }
            // Copy the content back to the buffer.
            memcpy(cache, buffer + fs->block_size * blocks_read - (offset % fs->block_size), end_size);
            // Write the block back.
            if (ext2_write_inode_block(fs, inode, inode_index, end_block, cache)) {
                pr_err("Failed to write the inode block `%d`\n", start_block);
                goto free_cache_return_error;
            }
        }
    }
    return size_to_write;
free_cache_return_error:
    // Free the cache.
    kmem_cache_free(cache);
    return -1;
}

// ============================================================================
// Directory Entry Iteration Functions
// ============================================================================

/// @brief Iterator for visiting the directory entries.
typedef struct ext2_direntry_iterator_t {
    ext2_filesystem_t *fs;   ///< A pointer to the filesystem.
    uint8_t *cache;          ///< Cache used for reading.
    ext2_inode_t *inode;     ///< A pointer to the directory inode.
    uint32_t block_index;    ///< The current block we are reading.
    uint32_t total_offset;   ///< The total amount of bytes we have read.
    uint32_t block_offset;   ///< The total amount of bytes we have read inside the current block.
    ext2_dirent_t *direntry; ///< Pointer to the directory entry.
} ext2_direntry_iterator_t;

/// @brief Returns the ext2_dirent_t pointed by the iterator.
/// @param iterator the iterator.
/// @return pointer to the ext2_dirent_t
ext2_dirent_t *ext2_direntry_iterator_get(ext2_direntry_iterator_t *iterator)
{
    return (ext2_dirent_t *)((uintptr_t)iterator->cache + iterator->block_offset);
}

/// @brief Check if the iterator is valid.
/// @param iterator the iterator to check.
/// @return true if valid, false otherwise.
bool_t ext2_direntry_iterator_valid(ext2_direntry_iterator_t *iterator)
{
    return iterator->direntry != NULL;
}

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
        .direntry     = NULL
    };
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
/// @param iterator the iterator.
void ext2_direntry_iterator_next(ext2_direntry_iterator_t *iterator)
{
    // Get the current rec_len.
    uint32_t rec_len = ext2_direntry_iterator_get(iterator)->rec_len;
    // Advance the offsets.
    iterator->block_offset += rec_len;
    iterator->total_offset += rec_len;
    // If we reached the end of the inode, stop.
    if (iterator->total_offset >= iterator->inode->size) {
        // The iterator is not valid anymore.
        iterator->direntry = NULL;
        return;
    }
    // If we exceed the size of a block, move to the next block.
    if (iterator->block_offset >= iterator->fs->block_size) {
        // Increase the block index.
        iterator->block_index += 1;
        // Remove the exceeding size, so that we start correctly in the new block.
        iterator->block_offset -= iterator->fs->block_size;
        // Read the new block.
        if (ext2_read_inode_block(iterator->fs, iterator->inode, iterator->block_index, iterator->cache) == -1) {
            pr_err("Failed to read the inode block `%d`\n", iterator->block_index);
            // The iterator is not valid anymore.
            iterator->direntry = NULL;
            return;
        }
    }
    // Read the direntry.
    iterator->direntry = ext2_direntry_iterator_get(iterator);
}

// ============================================================================
// Directory Entry Management Functions
// ============================================================================

static int ext2_allocate_direntry(ext2_filesystem_t *fs, vfs_file_t *parent, char *name, uint32_t inode_index)
{
    pr_err("Not implemented yet.\n");
    return -1;
}

/// @brief Finds the entry with the given `name` inside the `directory`.
/// @param directory the directory in which we perform the search.
/// @param name the name of the entry we are looking for.
/// @param search the output variable where we save the info about the entry.
/// @return 0 on success, -1 on failure.
static int ext2_find_entry(ext2_filesystem_t *fs, ino_t ino, const char *name, ext2_direntry_search_t *search)
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
    if (search->direntry == NULL) {
        pr_err("You provided a NULL direntry.\n");
        return -1;
    }
    // Allocate the cache.
    uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    // Clean the cache.
    memset(cache, 0, fs->block_size);
    // Get the inode associated with the file.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, ino) == -1) {
        pr_err("Failed to read the inode (%d).\n", ino);
        goto free_cache_return_error;
    }
    ext2_direntry_iterator_t it = ext2_direntry_iterator_begin(fs, cache, &inode);
    for (; ext2_direntry_iterator_valid(&it); ext2_direntry_iterator_next(&it)) {
        // Chehck the name.
        if (!strcmp(it.direntry->name, ".") && !strcmp(name, "/")) {
            break;
        }
        // Check if the entry has the same name.
        if ((it.direntry->inode != 0) && (strlen(name) == it.direntry->name_len))
            if (!strncmp(it.direntry->name, name, it.direntry->name_len))
                break;
    }
    // Copy the inode of the parent, even if we did not find the entry.
    search->parent_inode = ino;
    // Check if we have found the entry.
    if (it.direntry == NULL)
        goto free_cache_return_error;
    // Copy the direntry.
    memcpy(search->direntry, it.direntry, sizeof(ext2_dirent_t));
    // Copy the index of the block containing the direntry.
    search->block_index = it.block_index;
    // Copy the offset of the direntry inside the block.
    search->block_offset = it.block_offset;
    // Free the cache.
    kmem_cache_free(cache);
    return 0;
free_cache_return_error:
    // Free the cache.
    kmem_cache_free(cache);
    return -1;
}

/// @brief Searches the entry specified in `path` starting from `directory`.
/// @param directory the directory from which we start performing the search.
/// @param path the path of the entry we are looking for, it cna be a relative path.
/// @param search the output variable where we save the entry information.
/// @return 0 on success, -1 on failure.
static int ext2_resolve_path(vfs_file_t *directory, char *path, ext2_direntry_search_t *search)
{
    pr_debug("ext2_resolve_path(%s, %s, %p)\n", directory->name, path, search);
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
    if (search->direntry == NULL) {
        pr_err("You provided a NULL direntry.\n");
        return -1;
    }
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)directory->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", directory->name);
        return -1;
    }
    // If the path is `/`.
    if (strcmp(path, "/") == 0)
        return ext2_find_entry(fs, directory->ino, path, search);
    ino_t ino   = directory->ino;
    char *token = strtok(path, "/");
    while (token) {
        if (!ext2_find_entry(fs, ino, token, search)) {
            ino = search->direntry->inode;
        } else {
            memset(search->direntry, 0, sizeof(ext2_dirent_t));
            return -1;
        }
        token = strtok(NULL, "/");
    }
    return 0;
}

/// @brief Searches the entry specified in `path` starting from `directory`.
/// @param directory the directory from which we start performing the search.
/// @param path the path of the entry we are looking for, it cna be a relative path.
/// @param direntry the output variable where we save the found entry.
/// @return 0 on success, -1 on failure.
static int ext2_resolve_path_direntry(vfs_file_t *directory, char *path, ext2_dirent_t *direntry)
{
    pr_debug("ext2_resolve_path_direntry(%s, %s, %p)\n", directory->name, path, direntry);
    // Check the pointers.
    if (directory == NULL) {
        pr_err("You provided a NULL directory.\n");
        return -1;
    }
    if (path == NULL) {
        pr_err("You provided a NULL path.\n");
        return -1;
    }
    if (direntry == NULL) {
        pr_err("You provided a NULL direntry.\n");
        return -1;
    }
    // Prepare the structure for the search.
    ext2_direntry_search_t search;
    memset(&search, 0, sizeof(ext2_direntry_search_t));
    // Initialize the search structure.
    search.direntry     = direntry;
    search.block_index  = 0;
    search.block_offset = 0;
    search.parent_inode = 0;
    return ext2_resolve_path(directory, path, &search);
}

/// @brief Get the ext2 filesystem object starting from a path.
/// @param absolute_path the absolute path for which we want to find the associated EXT2 filesystem.
/// @return a pointer to the EXT2 filesystem, NULL otherwise.
static ext2_filesystem_t *get_ext2_filesystem(const char *absolute_path)
{
    pr_debug("get_ext2_filesystem(%s)\n", absolute_path);
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

/// @brief Sets the filesystem root.
/// @param fs the filesystem.
/// @param inode the inode we use to initialize the root of the filesystem.
/// @return 0 on success, -1 on failure.
static int ext2_init_root(ext2_filesystem_t *fs, ext2_inode_t *inode, const char *path)
{
    // Information for root dir.
    fs->root->device = (void *)fs;
    fs->root->ino    = 2;
    // Copy the path where the root is mounted.
    strcpy(fs->root->name, path);
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
        return -1;
    }
    if ((inode->mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
        fs->root->flags |= DT_DIR;
    } else {
        pr_err("The root inode is not a directory.\n");
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
    fs->root->sys_operations = &ext2_sys_operations;
    fs->root->fs_operations  = &ext2_fs_operations;
    // Initialize the list of siblings.
    list_head_init(&fs->root->siblings);
    return 0;
}

static int ext2_init_vfs_file(
    ext2_filesystem_t *fs,
    vfs_file_t *file,
    ext2_inode_t *inode,
    uint32_t inode_index,
    const char *name,
    size_t name_len)
{
    // Information for root dir.
    file->device = (void *)fs;
    file->ino    = inode_index;
    // Copy the name
    memcpy(&file->name, name, name_len);
    // Information from the inode.
    file->uid    = inode->uid;
    file->gid    = inode->gid;
    file->length = inode->size;
    file->mask   = inode->mode & 0xFFF;
    file->nlink  = inode->links_count;
    // File flags.
    file->flags = 0;
    if ((inode->mode & EXT2_S_IFREG) == EXT2_S_IFREG) {
        file->flags |= DT_REG;
    }
    if ((inode->mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
        file->flags |= DT_DIR;
    }
    if ((inode->mode & EXT2_S_IFBLK) == EXT2_S_IFBLK) {
        file->flags |= DT_BLK;
    }
    if ((inode->mode & EXT2_S_IFCHR) == EXT2_S_IFCHR) {
        file->flags |= DT_CHR;
    }
    if ((inode->mode & EXT2_S_IFIFO) == EXT2_S_IFIFO) {
        file->flags |= DT_FIFO;
    }
    if ((inode->mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
        file->flags |= DT_LNK;
    }
    file->atime          = inode->atime;
    file->mtime          = inode->mtime;
    file->ctime          = inode->ctime;
    file->sys_operations = &ext2_sys_operations;
    file->fs_operations  = &ext2_fs_operations;
    // Initialize the list of siblings.
    list_head_init(&file->siblings);
    return 0;
}

static vfs_file_t *ext2_find_vfs_file_with_inode(ext2_filesystem_t *fs, ino_t inode)
{
    vfs_file_t *file = NULL;
    if (!list_head_empty(&fs->opened_files)) {
        list_for_each_decl(it, &fs->opened_files)
        {
            // Get the file structure.
            file = list_entry(it, vfs_file_t, siblings);
            if (file && (file->ino == inode))
                return file;
        }
    }
    return NULL;
}

// ============================================================================
// Virtual FileSystem (VFS) Functions
// ============================================================================

/// @brief Creates and initializes a new inode.
/// @param fs the filesystem.
/// @param inode the inode we use to initialize the root of the filesystem.
/// @param preferred_group the preferred group where the inode should be allocated.
/// @return true on success, false on failure.
static bool_t ext2_create_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t preferred_group)
{
    if (fs == NULL) {
        pr_err("Received a null EXT2 filesystem.\n");
        return false;
    }
    if (inode == NULL) {
        pr_err("Received a null EXT2 inode.\n");
        return false;
    }
    task_struct *task = scheduler_get_current_process();
    if (task == NULL) {
        pr_err("Failed to get the current running process.\n");
        return false;
    }
    // Allocate an inode, inside the preferred_group if possible.
    int inode_index = ext2_allocate_inode(fs, preferred_group);
    if (inode_index == 0) {
        pr_err("Failed to allocate a new inode.\n");
        return false;
    }
    // Clean the inode structure.
    memset(inode, 0, sizeof(ext2_inode_t));
    // Get the inode associated with the directory entry.
    if (ext2_read_inode(fs, inode, inode_index) == -1) {
        pr_err("Failed to read the newly created inode.\n");
        return false;
    }
    // Set the user identifiers of the owners.
    inode->uid = task->uid;
    // Set the size of the file in bytes.
    inode->size = 0;
    // Set the time that the inode was accessed.
    inode->atime = sys_time(NULL);
    // Set the time that the inode was created.
    inode->ctime = inode->atime;
    // Set the time that the inode was modified the last time.
    inode->mtime = inode->atime;
    // Set the time that the inode was deleted.
    inode->dtime = 0;
    // Set the group identifiers of the owners.
    inode->gid = task->gid;
    // Set the number of hard links.
    inode->links_count = 1;
    // Set the blocks count.
    inode->blocks_count = 0;
    // Set the file flags.
    inode->flags = 0;
    // Set the OS dependant value.
    inode->osd1 = 0;
    // Set the blocks data.
    memset(&inode->data, 0, sizeof(inode->data));
    // Set the value used to indicate the file version (used by NFS).
    inode->generation = 0;
    // TODO: The value indicating the block number containing the extended attributes.
    inode->file_acl = 0;
    // TODO: For regular files this 32bit value contains the high 32 bits of the 64bit file size.
    inode->dir_acl = 0;
    // TODO:Value indicating the location of the file fragment.
    inode->fragment_addr = 0;
    // TODO: OS dependant structure.
    memset(&inode->osd2, 0, sizeof(inode->osd2));
    // Write the inode.
    if (ext2_write_inode(fs, inode, inode_index) == -1) {
        pr_err("Failed to write the newly created inode.\n");
        return false;
    }
    return true;
}

static vfs_file_t *ext2_creat(vfs_file_t *parent, const char *name, mode_t mode)
{
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)parent->device;
    if (fs == NULL) {
        pr_err("The parent does not belong to an EXT2 filesystem `%s`.\n", parent->name);
        return NULL;
    }
    pr_err("Not implemented yet.\n");

#if 0
    // Get the group index of the parent.
    uint32_t group_index = ext2_get_group_index_from_inode(fs, parent->ino);
    // Create and initialize the new inode.
    ext2_inode_t inode;
    if (!ext2_create_inode(fs, &inode, group_index)) {
        pr_err("Failed to create a new inode inside `%s` (group index: %d).\n", parent->name, group_index);
        return NULL;
    }
    // Initialize the file.
    ext2_allocate_direntry(fs, parent, name, inode_index);
#endif
    return NULL;
}

/// @brief Open the file at the given path and returns its file descriptor.
/// @param path  The path to the file.
/// @param flags The flags used to determine the behavior of the function.
/// @param mode  The mode with which we open the file.
/// @return The file descriptor of the opened file, otherwise returns -1.
static vfs_file_t *ext2_open(const char *path, int flags, mode_t mode)
{
    pr_debug("ext2_open(%s, %d, %d)\n", path, flags, mode);
    // Get the absolute path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    if (!realpath(path, absolute_path)) {
        pr_err("Cannot get the absolute path for path `%s`.\n", path);
        return NULL;
    }
    // Get the EXT2 filesystem.
    ext2_filesystem_t *fs = get_ext2_filesystem(absolute_path);
    if (fs == NULL) {
        pr_err("Failed to get the EXT2 filesystem for absolute path `%s`.\n", absolute_path);
        return NULL;
    }
    // Prepare the structure for the direntry.
    ext2_dirent_t direntry;
    memset(&direntry, 0, sizeof(ext2_dirent_t));
    // Prepare the structure for the search.
    ext2_direntry_search_t search;
    memset(&search, 0, sizeof(ext2_direntry_search_t));
    // Initialize the search structure.
    search.direntry     = &direntry;
    search.block_index  = 0;
    search.block_offset = 0;
    search.parent_inode = 0;
    // First check, if a file with the given name already exists.
    if (!ext2_resolve_path(fs->root, absolute_path, &search)) {
        if (bitmask_check(flags, O_CREAT | O_EXCL)) {
            pr_err("A file at `%s` already exists (O_CREAT | O_EXCL).\n", absolute_path);
            return NULL;
        }
    } else {
        // If we need to create it, it's ok if it does not exist.
        if (bitmask_check(flags, O_CREAT)) {
            pr_warning("We want to create a new file at `%s`.\n", path);
            pr_warning("The inode of the parent is `%d`.\n", search.parent_inode);
#if 0
            // Prepare the structure for the inode.
            ext2_inode_t parent_inode;
            memset(&parent_inode, 0, sizeof(ext2_inode_t));
            // Get the inode associated with the directory entry.
            if (ext2_read_inode(fs, &parent_inode, search.parent_inode) == -1) {
                pr_err("Failed to read the inode of the parent `%d`.\n", search.parent_inode);
                return NULL;
            }
            // Search for the parent VFS file.
            vfs_file_t *parent = ext2_find_vfs_file_with_inode(fs, search.parent_inode);
            if (parent == NULL) {
                // Allocate the memory for the parent file.
                parent = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
                if (parent == NULL) {
                    pr_err("Failed to allocate memory for the parent.\n");
                    return NULL;
                }
                if (ext2_init_vfs_file(fs, &parent_inode, &direntry, parent) == -1) {
                    pr_err("Failed to properly set the VFS file.\n");
                    kmem_cache_free(parent);
                    return NULL;
                }
                // Add the vfs_file to the list of associated files.
                list_head_add_tail(&parent->siblings, &fs->opened_files);
                // Create the file.
                vfs_file_t *new_file = ext2_creat(parent, basename(path), mode);
                // Remove the parent from the list of opened VFS files.
                list_head_del(&parent->siblings);
                // Destroy the VFS file.
                kmem_cache_free(parent);
                return new_file;
            } else {
                return ext2_creat(parent, basename(path), mode);
            }
#endif
            return NULL;
        } else {
            pr_err("Failed to resolve absolute path `%s`.\n", absolute_path);
            return NULL;
        }
    }
    // Prepare the structure for the inode.
    ext2_inode_t inode;
    memset(&inode, 0, sizeof(ext2_inode_t));
    // Get the inode associated with the directory entry.
    if (ext2_read_inode(fs, &inode, direntry.inode) == -1) {
        pr_err("Failed to read the inode of `%s`.\n", direntry.name);
        return NULL;
    }
    vfs_file_t *file = ext2_find_vfs_file_with_inode(fs, direntry.inode);
    if (file == NULL) {
        // Allocate the memory for the file.
        file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
        if (file == NULL) {
            pr_err("Failed to allocate memory for the EXT2 file.\n");
            return NULL;
        }
        if (ext2_init_vfs_file(fs, file, &inode, direntry.inode, direntry.name, direntry.name_len) == -1) {
            pr_err("Failed to properly set the VFS file.\n");
            return NULL;
        }
        // Add the vfs_file to the list of associated files.
        list_head_add_tail(&file->siblings, &fs->opened_files);
    }
    return file;
}

static int ext2_unlink(const char *path)
{
    pr_debug("ext2_unlink(%s)\n", path);
    // Get the absolute path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    if (!realpath(path, absolute_path)) {
        pr_err("Cannot get the absolute path for path `%s`.\n", path);
        return -ENOENT;
    }
    // Get the name of the entry we want to unlink.
    char *name = basename(absolute_path);
    if (name == NULL) {
        pr_err("Cannot get the basename from the absolute path `%s`.\n", absolute_path);
        return -ENOENT;
    }
    // Get the EXT2 filesystem.
    ext2_filesystem_t *fs = get_ext2_filesystem(absolute_path);
    if (fs == NULL) {
        pr_err("Failed to get the EXT2 filesystem for absolute path `%s`.\n", absolute_path);
        return -ENOENT;
    }
    // Prepare the structure for the direntry.
    ext2_dirent_t direntry;
    memset(&direntry, 0, sizeof(ext2_dirent_t));
    // Prepare the structure for the search.
    ext2_direntry_search_t search;
    memset(&search, 0, sizeof(ext2_direntry_search_t));
    // Initialize the search structure.
    search.direntry     = &direntry;
    search.block_index  = 0;
    search.block_offset = 0;
    search.parent_inode = 0;
    // Resolve the path to the directory entry.
    if (ext2_resolve_path(fs->root, absolute_path, &search)) {
        pr_err("Failed to resolve path `%s`.\n", absolute_path);
        return -ENOENT;
    }
    // Get the inode associated with the parent directory entry.
    ext2_inode_t parent_inode;
    if (ext2_read_inode(fs, &parent_inode, search.parent_inode) == -1) {
        pr_err("ext2_stat(%s): Failed to read the inode of parent of `%s`.\n", path, direntry.name);
        return -ENOENT;
    }
    // Allocate the cache and clean it.
    uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    memset(cache, 0, fs->block_size);
    // Read the block where the direntry resides.
    if (ext2_read_inode_block(fs, &parent_inode, search.block_index, cache) == -1) {
        pr_err("Failed to read the parent inode block `%d`\n", search.block_index);
        goto free_cache_return_error;
    }
    // Get a pointer to the direntry.
    ext2_dirent_t *actual_dirent = (ext2_dirent_t *)((uintptr_t)cache + search.block_offset);
    if (actual_dirent == NULL) {
        pr_err("We found a NULL ext2_dirent_t\n");
        goto free_cache_return_error;
    }
    // Set the inode to zero.
    actual_dirent->inode = 0;
    // Write back the parent directory block.
    if (!ext2_write_inode_block(fs, &parent_inode, search.parent_inode, search.block_index, cache)) {
        pr_err("Failed to write the inode block `%d`\n", search.block_index);
        goto free_cache_return_error;
    }
    // Read the inode of the direntry we want to unlink.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, direntry.inode) == -1) {
        pr_err("Failed to read the inode of `%s`.\n", direntry.name);
        goto free_cache_return_error;
    }
    if (inode.links_count > 0) {
        inode.links_count--;
        // Update the inode.
        if (ext2_write_inode(fs, &inode, direntry.inode) == -1) {
            pr_err("Failed to update the inode of `%s`.\n", direntry.name);
            goto free_cache_return_error;
        }
    }
    // Free the cache.
    kmem_cache_free(cache);
    return 0;
free_cache_return_error:
    // Free the cache.
    kmem_cache_free(cache);
    return -1;
}

/// @brief Closes the given file.
/// @param file The file structure.
static int ext2_close(vfs_file_t *file)
{
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", file->name);
        return -1;
    }
    // We cannot close the root.
    if (file == fs->root) {
        return -1;
    }
    pr_debug("ext2_close(%p) : Closing file `%s`\n", file, file->name);
    // Remove the file from the list of opened files.
    list_head_del(&file->siblings);
    // Free the cache.
    kmem_cache_free(file);
    return 0;
}

/// @brief Reads from the file identified by the file descriptor.
/// @param file The file.
/// @param buffer Buffer where the read content must be placed.
/// @param offset Offset from which we start reading from the file.
/// @param nbyte The number of bytes to read.
/// @return The number of red bytes.
static ssize_t ext2_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte)
{
    //pr_debug("ext2_read(%s, %p, %d, %d)\n", file->name, buffer, offset, nbyte);
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", file->name);
        return -1;
    }
    // Get the inode associated with the file.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, file->ino) == -1) {
        pr_err("Failed to read the inode `%s`.\n", file->name);
        return -1;
    }
    return ext2_read_inode_data(fs, &inode, file->ino, offset, nbyte, buffer);
}

/// @brief Writes the given content inside the file.
/// @param file The file descriptor of the file.
/// @param buffer The content to write.
/// @param offset Offset from which we start writing in the file.
/// @param nbyte The number of bytes to write.
/// @return The number of written bytes.
static ssize_t ext2_write(vfs_file_t *file, const void *buffer, off_t offset, size_t nbyte)
{
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", file->name);
        return -1;
    }
    // Get the inode associated with the file.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, file->ino) == -1) {
        pr_err("Failed to read the inode `%s`.\n", file->name);
        return -1;
    }
    return ext2_write_inode_data(fs, &inode, file->ino, offset, nbyte, (char *)buffer);
}

/// @brief Repositions the file offset inside a file.
/// @param file the file we are working with.
/// @param offset the offest to use for the operation.
/// @param whence the type of operation.
/// @return  Upon successful completion, returns the resulting offset
/// location as measured in bytes from the beginning of the file. On
/// error, the value (off_t) -1 is returned and errno is set to
/// indicate the error.
static off_t ext2_lseek(vfs_file_t *file, off_t offset, int whence)
{
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
    // Deal with the specific whence.
    switch (whence) {
    case SEEK_END:
        offset += inode.size;
        break;
    case SEEK_CUR:
        if (offset == 0) {
            return file->f_pos;
        }
        offset += file->f_pos;
        break;
    case SEEK_SET:
        break;
    default:
        return -EINVAL;
    }
    if (offset >= 0) {
        if (offset != file->f_pos) {
            file->f_pos = offset;
        }
        return offset;
    }
    return -EINVAL;
}

/// @brief Saves the information concerning the file.
/// @param inode The inode containing the data.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int __ext2_stat(ext2_inode_t *inode, stat_t *stat)
{
    stat->st_mode  = inode->mode;
    stat->st_uid   = inode->uid;
    stat->st_gid   = inode->gid;
    stat->st_size  = inode->size;
    stat->st_atime = inode->atime;
    stat->st_mtime = inode->mtime;
    stat->st_ctime = inode->ctime;
    return 0;
}

/// @brief Retrieves information concerning the file at the given position.
/// @param file The file struct.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int ext2_fstat(vfs_file_t *file, stat_t *stat)
{
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
    /// ID of device containing file.
    stat->st_dev = fs->block_device->ino;
    // Set the inode.
    stat->st_ino = file->ino;
    // Set the rest of the structure.
    return __ext2_stat(&inode, stat);
}

static int ext2_ioctl(vfs_file_t *file, int request, void *data)
{
    return -1;
}

/// @brief Reads contents of the directories to a dirent buffer, updating
///        the offset and returning the number of written bytes in the buffer,
///        it assumes that all paths are well-formed.
/// @param file  The directory handler.
/// @param dirp  The buffer where the data should be written.
/// @param doff  The offset inside the buffer where the data should be written.
/// @param count The maximum length of the buffer.
/// @return The number of written bytes in the buffer.
static int ext2_getdents(vfs_file_t *file, dirent_t *dirp, off_t doff, size_t count)
{
    pr_debug("ext2_getdents(%s, %p, %d, %d)\n", file->name, dirp, doff, count);
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", file->name);
        return -ENOENT;
    }
    // Get the inode associated with the file.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, file->ino) == -1) {
        pr_err("Failed to read the inode (%d).\n", file->ino);
        return -ENOENT;
    }
    uint32_t current = 0, written = 0;
    // Allocate the cache.
    uint8_t *cache = kmem_cache_alloc(fs->ext2_buffer_cache, GFP_KERNEL);
    // Clean the cache.
    memset(cache, 0, fs->block_size);
    // Initialize the iterator.
    ext2_direntry_iterator_t it = ext2_direntry_iterator_begin(fs, cache, &inode);
    for (; ext2_direntry_iterator_valid(&it) && (written < count); ext2_direntry_iterator_next(&it)) {
        // Skip if already provided.
        current += sizeof(dirent_t);
        if (current <= doff)
            continue;
        // Write on current directory entry data.
        dirp->d_ino  = it.direntry->inode;
        dirp->d_type = ext2_file_type_to_vfs_file_type(it.direntry->file_type);
        memset(dirp->d_name, 0, NAME_MAX);
        strncpy(dirp->d_name, it.direntry->name, it.direntry->name_len);
        dirp->d_off    = it.direntry->rec_len;
        dirp->d_reclen = it.direntry->rec_len;
        // Increment the amount written.
        written += sizeof(dirent_t);
        // Move to next writing position.
        ++dirp;
    }
    // Free the cache.
    kmem_cache_free(cache);
    return written;
}

static int ext2_mkdir(const char *path, mode_t mode)
{
    return -1;
}

static int ext2_rmdir(const char *path)
{
    return -1;
}

/// @brief Retrieves information concerning the file at the given position.
/// @param path The path where the file resides.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int ext2_stat(const char *path, stat_t *stat)
{
    pr_debug("ext2_stat(%s, %p)\n", path, stat);
    // Get the absolute path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    if (!realpath(path, absolute_path)) {
        pr_err("Cannot get the absolute path for path `%s`.\n", path);
        return -ENOENT;
    }
    // Get the EXT2 filesystem.
    ext2_filesystem_t *fs = get_ext2_filesystem(absolute_path);
    if (fs == NULL) {
        pr_err("Failed to get the EXT2 filesystem for absolute path `%s`.\n", absolute_path);
        return -ENOENT;
    }
    // Prepare the structure for the direntry.
    ext2_dirent_t direntry;
    memset(&direntry, 0, sizeof(ext2_dirent_t));
    // Resolve the path.
    if (ext2_resolve_path_direntry(fs->root, absolute_path, &direntry)) {
        pr_err("Failed to resolve path `%s`.\n", absolute_path);
        return -ENOENT;
    }
    // Get the inode associated with the directory entry.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, direntry.inode) == -1) {
        pr_err("ext2_stat(%s): Failed to read the inode of `%s`.\n", path, direntry.name);
        return -ENOENT;
    }
    /// ID of device containing file.
    stat->st_dev = fs->block_device->ino;
    // Set the inode.
    stat->st_ino = direntry.inode;
    // Set the rest of the structure.
    return __ext2_stat(&inode, stat);
}

/// @brief Mounts the block device as an EXT2 filesystem.
/// @param block_device the block device formatted as EXT2.
/// @return the VFS root node of the EXT2 filesystem.
static vfs_file_t *ext2_mount(vfs_file_t *block_device, const char *path)
{
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
    fs->ext2_buffer_cache = kmem_cache_create(
        "ext2_buffer_cache",
        fs->block_size,
        fs->block_size,
        GFP_KERNEL,
        NULL,
        NULL);
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

    // We need the root inode in order to set the root file.
    ext2_inode_t root_inode;
    if (ext2_read_inode(fs, &root_inode, 2U) == -1) {
        pr_err("Failed to set the root inode.\n");
        // Free the block_buffer, the block_groups and the filesystem.
        goto free_block_buffer;
    }

    // Allocate the memory for the root.
    fs->root = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
    if (!fs->root) {
        pr_err("Failed to allocate memory for the EXT2 root file!\n");
        // Free the block_buffer, the block_groups and the filesystem.
        goto free_block_buffer;
    }

    if (ext2_init_root(fs, &root_inode, path) == -1) {
        pr_err("Failed to set the EXT2 root.\n");
        // Free the block_buffer, the block_groups and the filesystem.
        goto free_all;
    }
    // Add the root to the list of opened files.
    list_head_add_tail(&fs->root->siblings, &fs->opened_files);

    // Dump the filesystem details for debugging.
    ext2_dump_filesystem(fs);
    // Dump the superblock details for debugging.
    ext2_dump_superblock(&fs->superblock);
    // Dump the block group descriptor table.
    ext2_dump_bgdt(fs);

    return fs->root;

free_all:
    // Free the memory occupied by the root.
    kmem_cache_free(fs->root);
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

// ============================================================================
// Initialization Functions
// ============================================================================

static vfs_file_t *ext2_mount_callback(const char *path, const char *device)
{
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    if (!realpath(device, absolute_path)) {
        pr_err("ext2_mount_callback(%s, %s): Cannot get the absolute path.", path, device);
        return NULL;
    }
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("ext2_mount_callback(%s, %s): Cannot find the superblock at absolute path `%s`!\n", path, device, absolute_path);
        return NULL;
    }
    vfs_file_t *block_device = sb->root;
    if (block_device == NULL) {
        pr_err("ext2_mount_callback(%s, %s): Cannot find the superblock root.", path, device);
        return NULL;
    }
    if (block_device->flags != DT_BLK) {
        pr_err("ext2_mount_callback(%s, %s): The device is not a block device.\n", path, device);
        return NULL;
    }
    return ext2_mount(block_device, path);
}

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