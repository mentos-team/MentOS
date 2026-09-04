/// @file ext2_internal.h
/// @brief Definitions shared by the translation units of the EXT2 driver.
/// @details This header is private to kernel/src/fs/ext2: the public surface of
/// the driver is the two functions of fs/ext2.h, and nothing here is meant to
/// leave the directory. It carries the include prelude the whole module needs,
/// so that each translation unit opens with the logging setup and this file
/// alone.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#ifdef ENABLE_EXT2_TRACE
#include "resource_tracing.h"
/// @brief Tracks the resource registered by the driver, defined in ext2.c.
extern int ext2_resource_id;
#endif

// If defined, EXT2 will debug everything. The units of the driver test it,
// so it has to be visible to all of them from here.
// #define EXT2_FULL_DEBUG

#include "assert.h"
#include "errno.h"
#include "fcntl.h"
#include "fs/ext2.h"
#include "fs/vfs.h"
#include "fs/vfs_types.h"
#include "klib/spinlock.h"
#include "libgen.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "stdio.h"
#include "string.h"
#include "sys/stat.h"

#define EXT2_SUPERBLOCK_MAGIC  0xEF53 ///< Magic value used to identify an ext2 filesystem.
#define EXT2_DIRECT_BLOCKS     12     ///< Amount of indirect blocks in an inode.
#define EXT2_PATH_MAX          4096   ///< Maximum length of a pathname.
#define EXT2_MAX_SYMLINK_COUNT 8      ///< Maximum nesting of symlinks, used to prevent a loop.
#define EXT2_NAME_LEN          255    ///< The lenght of names inside directory entries.

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

/// @brief Types of file in an EXT2 filesystem.
typedef enum ext2_file_type {
    ext2_file_type_unknown,          ///< Unknown type.
    ext2_file_type_regular_file,     ///< Regular file.
    ext2_file_type_directory,        ///< Directory.
    ext2_file_type_character_device, ///< Character device.
    ext2_file_type_block_device,     ///< Block device.
    ext2_file_type_named_pipe,       ///< Named pipe.
    ext2_file_type_socket,           ///< Socket
    ext2_file_type_symbolic_link     ///< Symbolic link.
} ext2_file_type_t;

/// @brief The superblock contains all the information about the configuration
/// of the filesystem.
/// @details The primary copy of the superblock is stored at an offset of 1024
/// bytes from the start of the device, and it is essential to mounting the
/// filesystem. Since it is so important, backup copies of the superblock are
/// stored in block groups throughout the filesystem.
typedef struct ext2_superblock {
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

/// @brief Entry of the Block Group Descriptor Table (BGDT).
typedef struct ext2_group_descriptor {
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
typedef struct ext2_inode {
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
    /// @brief Mutable data.
    union {
        /// [60 byte] Blocks indices.
        struct {
            /// [48 byte]
            uint32_t dir_blocks[EXT2_DIRECT_BLOCKS];
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
typedef struct ext2_dirent {
    /// Number of the inode that this directory entry points to.
    uint32_t inode;
    /// Length of this directory entry. Must be a multiple of 4.
    uint16_t rec_len;
    /// Length of the file name.
    uint8_t name_len;
    /// File type code.
    uint8_t file_type;
    /// File name, of maximum EXT2_NAME_LEN length.
    char name[];
} ext2_dirent_t;

/// @brief The details regarding the filesystem.
typedef struct ext2_filesystem {
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
    list_head_t opened_files;

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

    /// Spinlock for protecting filesystem operations.
    spinlock_t spinlock;
} ext2_filesystem_t;

/// @brief Structure used when searching for a directory entry.
typedef struct ext2_direntry_search {
    /// The inode of the parent directory.
    ino_t parent_inode;
    /// The index of the block where the direntry resides.
    uint32_t block_index;
    /// The offest of the direntry inside the block.
    uint32_t block_offset;
    /// The direntry where we store the search results, this one has a name of maximum size.
    struct {
        /// Number of the inode that this directory entry points to.
        uint32_t inode;
        /// Length of this directory entry. Must be a multiple of 4.
        uint16_t rec_len;
        /// Length of the file name.
        uint8_t name_len;
        /// File type code.
        uint8_t file_type;
        /// File name of maximum size.
        char name[EXT2_NAME_LEN];
    } direntry;
} ext2_direntry_search_t;

/// @brief Iterator for visiting the directory entries.
typedef struct ext2_direntry_iterator {
    ext2_filesystem_t *fs;   ///< A pointer to the filesystem.
    uint8_t *cache;          ///< Cache used for reading.
    ext2_inode_t *inode;     ///< A pointer to the directory inode.
    uint32_t block_index;    ///< The current block we are reading.
    uint32_t total_offset;   ///< The total amount of bytes we have read.
    uint32_t block_offset;   ///< The total amount of bytes we have read inside the current block.
    ext2_dirent_t *direntry; ///< Pointer to the directory entry.
} ext2_direntry_iterator_t;

// ============================================================================
// Small helpers, kept inline so that every unit can use them
// ============================================================================

/// @brief Returns the rec_len from the given name.
/// @param name the name we use to compute the rec_len.
/// @return the rec_len value.
static inline uint32_t ext2_get_rec_len_from_name(const char *name)
{
    // Validate input.
    if (!name) {
        pr_err("Invalid input: name is NULL.");
        return 0;
    }

    // Compute and return the record length.
    return round_up(sizeof(ext2_dirent_t) + strlen(name) + 1, 4);
}

/// @brief Returns the rec_len from the given direntry.
/// @param direntry the direntry we use to compute the rec_len.
/// @return the rec_len value.
static inline uint32_t ext2_get_rec_len_from_direntry(const ext2_dirent_t *direntry)
{
    return round_up(sizeof(ext2_dirent_t) + direntry->name_len, 4);
}

/// @brief If the real rec_len is different from the on in attribute rec_len,
/// this is the last directory entry.
/// @param direntry the directory entry to check.
/// @return 1 if it is the last, 0 otherwise.
static inline uint32_t ext2_is_last_directory_entry(const ext2_dirent_t *direntry)
{
    return direntry->rec_len != ext2_get_rec_len_from_direntry(direntry);
}

/// @brief Cheks if the bit at the given linear index is free.
/// @param buffer the buffer containing the bitmap
/// @param offset the linear index we want to check.
/// @return if the bit is 0 or 1.
/// @details
/// How we access the specific bits inside the bitmap takes inspiration from the
/// mailman's algorithm.
static inline int ext2_bitmap_check(uint8_t *buffer, uint32_t offset)
{
    return (buffer[offset / 8]) & (1U << (offset % 8));
}

/// @brief Sets the bit at the given index.
/// @param buffer the buffer containing the bitmap
/// @param offset the bit index we want to set.
static inline void ext2_bitmap_set(uint8_t *buffer, uint32_t offset) { buffer[offset / 8] |= (1U << (offset % 8)); }

/// @brief Clears the bit at the given index.
/// @param buffer the buffer containing the bitmap
/// @param offset the bit index we want to clear.
static inline void ext2_bitmap_clear(uint8_t *buffer, uint32_t offset) { buffer[offset / 8] &= ~(1U << (offset % 8)); }

/// @brief Determining which block group contains an inode.
/// @param fs the ext2 filesystem structure.
/// @param inode_index the inode index.
/// @return the group index.
/// @details Remember that inode addressing starts from 1.
static inline uint32_t ext2_inode_index_to_group_index(ext2_filesystem_t *fs, uint32_t inode_index)
{
    assert(inode_index != 0 && "Your are trying to access inode 0.");
    return (inode_index - 1) / fs->superblock.inodes_per_group;
}

/// @brief Determining the offest of the inode inside the inode group.
/// @param fs the ext2 filesystem structure.
/// @param inode_index the inode index.
/// @return the offset of the inode inside the group.
/// @details Remember that inode addressing starts from 1.
static inline uint32_t ext2_inode_index_to_group_offset(ext2_filesystem_t *fs, uint32_t inode_index)
{
    assert(inode_index != 0 && "Your are trying to access inode 0.");
    return (inode_index - 1) % fs->superblock.inodes_per_group;
}

/// @brief Determining the block index from the inode index.
/// @param fs the ext2 filesystem structure.
/// @param inode_index the inode index.
/// @return the block index.
static inline uint32_t ext2_inode_index_to_block_index(ext2_filesystem_t *fs, uint32_t inode_index)
{
    assert(inode_index != 0 && "Your are trying to access inode 0.");
    return (ext2_inode_index_to_group_offset(fs, inode_index) * fs->superblock.inode_size) / fs->block_size;
}

/// @brief Determining which block group contains a given block.
/// @param fs the ext2 filesystem structure.
/// @param block_index the block index.
/// @return the block group index.
static inline uint32_t ext2_block_index_to_group_index(ext2_filesystem_t *fs, uint32_t block_index)
{
    return block_index / fs->superblock.blocks_per_group;
}

/// @brief Determining the offest of the block inside the block group.
/// @param fs the ext2 filesystem structure.
/// @param block_index the block index.
/// @return the offset of the block inside the group.
static inline uint32_t ext2_block_index_to_group_offset(ext2_filesystem_t *fs, uint32_t block_index)
{
    return block_index % fs->superblock.blocks_per_group;
}

// ============================================================================
// The VFS operation tables, defined in ext2.c
// ============================================================================

/// Filesystem general operations.
extern vfs_sys_operations_t ext2_sys_operations;
/// Filesystem file operations.
extern vfs_file_operations_t ext2_fs_operations;

// ============================================================================
// Functions shared between the units of the driver
// ============================================================================

// Defined in ext2_debug.c.
void ext2_dump_superblock(ext2_superblock_t *sb);
void ext2_dump_dirent(ext2_dirent_t *dirent);
void ext2_dump_bgdt(ext2_filesystem_t *fs);
void ext2_dump_filesystem(ext2_filesystem_t *fs);
void ext2_dump_direntries(ext2_filesystem_t *fs, uint8_t *cache, ext2_inode_t *parent_inode);

// Defined in ext2_io.c.
uint8_t *ext2_alloc_cache(ext2_filesystem_t *fs);
void ext2_dealloc_cache(uint8_t *cache);
int ext2_read_superblock(ext2_filesystem_t *fs);
int ext2_write_superblock(ext2_filesystem_t *fs);
int ext2_read_block(ext2_filesystem_t *fs, uint32_t block_index, uint8_t *buffer);
int ext2_write_block(ext2_filesystem_t *fs, uint32_t block_index, uint8_t *buffer);
int ext2_read_bgdt(ext2_filesystem_t *fs);
int ext2_write_bgdt_for_group(ext2_filesystem_t *fs, uint32_t group_index);
int ext2_read_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index);
int ext2_write_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index);

// Defined in ext2_alloc.c.
int ext2_allocate_inode(ext2_filesystem_t *fs, unsigned preferred_group);
uint32_t ext2_allocate_block(ext2_filesystem_t *fs);
void ext2_free_block(ext2_filesystem_t *fs, uint32_t block_index);
int ext2_free_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index);

// Defined in ext2_blockmap.c.
uint32_t ext2_get_real_block_index(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t block_index);
int ext2_allocate_inode_block(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index, uint32_t block_index);
ssize_t ext2_read_inode_block(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t block_index, uint8_t *buffer);
ssize_t ext2_write_inode_block(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index, uint32_t block_index, uint8_t *buffer);
ssize_t ext2_read_inode_data(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index, off_t offset, size_t nbyte, char *buffer);
ssize_t ext2_write_inode_data(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index, off_t offset, size_t nbyte, char *buffer);
int ext2_clean_inode_content(ext2_filesystem_t *fs, ext2_inode_t *inode, uint32_t inode_index);

// Defined in ext2_dir.c.
int ext2_direntry_iterator_valid(ext2_direntry_iterator_t *it);
ext2_direntry_iterator_t ext2_direntry_iterator_begin(ext2_filesystem_t *fs, uint8_t *cache, ext2_inode_t *inode);
void ext2_direntry_iterator_next(ext2_direntry_iterator_t *it);
int ext2_initialize_new_direntry_block(ext2_filesystem_t *fs, uint32_t inode_index, uint32_t block_index);
int ext2_allocate_direntry(ext2_filesystem_t *fs, uint32_t parent_inode_index, uint32_t direntry_inode_index, const char *name, uint8_t file_type);
int ext2_destroy_direntry(ext2_filesystem_t *fs, ext2_inode_t parent, ext2_inode_t inode, uint32_t parent_index, uint32_t inode_index, uint32_t block_index, uint32_t block_offset);
int ext2_find_direntry(ext2_filesystem_t *fs, ino_t ino, const char *name, ext2_direntry_search_t *search);

// Defined in ext2_namei.c.
int ext2_clear_direntry_for_path(ext2_filesystem_t *fs, const char *path);
int ext2_valid_x_permission(task_struct *task, ext2_inode_t *inode);
int ext2_file_type_to_vfs_file_type(int ext2_type);
int ext2_resolve_path(vfs_file_t *directory, const char *path, ext2_direntry_search_t *search);
ext2_filesystem_t *ext2_get_filesystem(const char *absolute_path);
void ext2_set_vfs_file_properties(ext2_filesystem_t *fs, vfs_file_t *file, ext2_inode_t *inode, uint32_t inode_index, const char *name, size_t name_len);
int ext2_init_vfs_file(ext2_filesystem_t *fs, vfs_file_t *file, ext2_inode_t *inode, uint32_t inode_index, const char *name, size_t name_len);
vfs_file_t *ext2_find_vfs_file_with_inode(ext2_filesystem_t *fs, ino_t inode);
int ext2_create_inode(ext2_filesystem_t *fs, ext2_inode_t *inode, mode_t mode, uint32_t preferred_group);
int ext2_unlink(const char *path);
int ext2_mkdir(const char *path, mode_t mode);
int ext2_rmdir(const char *path);

// Defined in ext2_file.c.
vfs_file_t *ext2_creat(const char *path, mode_t mode);
vfs_file_t *ext2_open(const char *path, int flags, mode_t mode);
int ext2_close(vfs_file_t *file);
ssize_t ext2_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte);
ssize_t ext2_write(vfs_file_t *file, const void *buffer, off_t offset, size_t nbyte);
off_t ext2_lseek(vfs_file_t *file, off_t offset, int whence);
long ext2_ioctl(vfs_file_t *file, unsigned int request, unsigned long data);
ssize_t ext2_getdents(vfs_file_t *file, dirent_t *dirp, off_t doff, size_t count);
ssize_t ext2_readlink(const char *path, char *buffer, size_t bufsize);

// Defined in ext2_attr.c.
int ext2_fstat(vfs_file_t *file, stat_t *stat);
int ext2_stat(const char *path, stat_t *stat);
int ext2_statfs(const char *path, statfs_t *statfs);
int ext2_fsetattr(vfs_file_t *file, struct iattr *attr);
int ext2_setattr(const char *path, struct iattr *attr);
