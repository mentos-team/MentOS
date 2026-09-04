/// @file ext2_debug.c
/// @brief EXT2 human-readable dumps of the on-disk structures.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[EXT2  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "ext2_internal.h"

/// @brief Turns an UUID to string.
/// @param uuid the UUID to turn to string.
/// @return the string representing the UUID.
static const char *uuid_to_string(uint8_t uuid[16])
{
    static char s[33] = {0};
    sprintf(
        s, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5], uuid[6], uuid[7], uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14],
        uuid[15]);
    return s;
}

/// @brief Turns an ext2_file_type to string.
/// @param ext2_type the ext2_file_type to turn to string.
/// @return the string representing the ext2_file_type.
static const char *ext2_file_type_to_string(ext2_file_type_t ext2_type)
{
    if (ext2_type == ext2_file_type_regular_file) {
        return "REG";
    }
    if (ext2_type == ext2_file_type_directory) {
        return "DIR";
    }
    if (ext2_type == ext2_file_type_character_device) {
        return "CHR";
    }
    if (ext2_type == ext2_file_type_block_device) {
        return "BLK";
    }
    if (ext2_type == ext2_file_type_named_pipe) {
        return "FIFO";
    }
    if (ext2_type == ext2_file_type_socket) {
        return "SOCK";
    }
    if (ext2_type == ext2_file_type_symbolic_link) {
        return "LNK";
    }
    return "UNK";
}

/// @brief Turns the time to string.
/// @param time the UNIX time to turn to string.
/// @return time turned to string.
static const char *time_to_string(uint32_t time)
{
    static char s[250] = {0};
    tm_t *tm           = localtime(&time);
    sprintf(s, "%2d/%2d %2d:%2d", tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min);
    return s;
}

/// @brief Dumps on debugging output the superblock.
/// @param sb the object to dump.
void ext2_dump_superblock(ext2_superblock_t *sb)
{
    pr_debug("inodes_count          : %u\n", sb->inodes_count);
    pr_debug("blocks_count          : %u\n", sb->blocks_count);
    pr_debug("r_blocks_count        : %u\n", sb->r_blocks_count);
    pr_debug("free_blocks_count     : %u\n", sb->free_blocks_count);
    pr_debug("free_inodes_count     : %u\n", sb->free_inodes_count);
    pr_debug("first_data_block      : %u\n", sb->first_data_block);
    pr_debug("log_block_size        : %u\n", sb->log_block_size);
    pr_debug("log_frag_size         : %u\n", sb->log_frag_size);
    pr_debug("blocks_per_group      : %u\n", sb->blocks_per_group);
    pr_debug("frags_per_group       : %u\n", sb->frags_per_group);
    pr_debug("inodes_per_group      : %u\n", sb->inodes_per_group);
    pr_debug("mtime                 : %s\n", time_to_string(sb->mtime));
    pr_debug("wtime                 : %s\n", time_to_string(sb->wtime));
    pr_debug("mnt_count             : %d\n", sb->mnt_count);
    pr_debug("max_mnt_count         : %d\n", sb->max_mnt_count);
    pr_debug("magic                 : 0x%0x (== 0x%0x)\n", sb->magic, EXT2_SUPERBLOCK_MAGIC);
    pr_debug("state                 : %d\n", sb->state);
    pr_debug("errors                : %d\n", sb->errors);
    pr_debug("minor_rev_level       : %d\n", sb->minor_rev_level);
    pr_debug("lastcheck             : %s\n", time_to_string(sb->lastcheck));
    pr_debug("checkinterval         : %u\n", sb->checkinterval);
    pr_debug("creator_os            : %u\n", sb->creator_os);
    pr_debug("rev_level             : %u\n", sb->rev_level);
    pr_debug("def_resuid            : %u\n", sb->def_resuid);
    pr_debug("def_resgid            : %u\n", sb->def_resgid);
    pr_debug("first_ino             : %u\n", sb->first_ino);
    pr_debug("inode_size            : %u\n", sb->inode_size);
    pr_debug("block_group_nr        : %u\n", sb->block_group_nr);
    pr_debug("feature_compat        : %u\n", sb->feature_compat);
    pr_debug("feature_incompat      : %u\n", sb->feature_incompat);
    pr_debug("feature_ro_compat     : %u\n", sb->feature_ro_compat);
    pr_debug("uuid                  : %s\n", uuid_to_string(sb->uuid));
    pr_debug("volume_name           : %s\n", (char *)sb->volume_name);
    pr_debug("last_mounted          : %s\n", (char *)sb->last_mounted);
    pr_debug("algo_bitmap           : %u\n", sb->algo_bitmap);
    pr_debug("prealloc_blocks       : %u\n", sb->prealloc_blocks);
    pr_debug("prealloc_dir_blocks   : %u\n", sb->prealloc_dir_blocks);
    pr_debug("journal_uuid          : %s\n", uuid_to_string(sb->journal_uuid));
    pr_debug("journal_inum          : %u\n", sb->journal_inum);
    pr_debug("jounral_dev           : %u\n", sb->jounral_dev);
    pr_debug("last_orphan           : %u\n", sb->last_orphan);
    pr_debug(
        "hash_seed             : %u %u %u %u\n", sb->hash_seed[0], sb->hash_seed[1], sb->hash_seed[2],
        sb->hash_seed[3]);
    pr_debug("def_hash_version      : %u\n", sb->def_hash_version);
    pr_debug("default_mount_options : %u\n", sb->default_mount_options);
    pr_debug("first_meta_bg         : %u\n", sb->first_meta_block_group_id);
}

/// @brief Dumps on debugging output the group descriptor.
/// @param gd the object to dump.
static void ext2_dump_group_descriptor(ext2_group_descriptor_t *gd)
{
    pr_debug("block_bitmap          : %u\n", gd->block_bitmap);
    pr_debug("inode_bitmap          : %u\n", gd->inode_bitmap);
    pr_debug("inode_table           : %u\n", gd->inode_table);
    pr_debug("free_blocks_count     : %u\n", gd->free_blocks_count);
    pr_debug("free_inodes_count     : %u\n", gd->free_inodes_count);
    pr_debug("used_dirs_count       : %u\n", gd->used_dirs_count);
}

/// @brief Dumps on debugging output the inode.
/// @param fs a pointer to the filesystem.
/// @param inode the object to dump.
static void ext2_dump_inode(ext2_filesystem_t *fs, ext2_inode_t *inode)
{
    char mask[32];
    tm_t *timeinfo;
    strmode(inode->mode, mask);
    pr_debug("   Size : %6u N. Blocks : %4u\n", inode->size, inode->blocks_count);
    pr_debug(
        " Access : (%4d/%s) Uid: (%d, %s) Gid: (%d, %s)\n", inode->mode, mask, inode->uid, "None", inode->gid, "None");
    timeinfo = localtime(&inode->atime);
    pr_debug(
        " Access : %4d-%02d-%02d %2d:%2d:%2d (%d)\n", timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday,
        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, inode->atime);
    timeinfo = localtime(&inode->mtime);
    pr_debug(
        " Modify : %4d-%02d-%02d %2d:%2d:%2d (%d)\n", timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday,
        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, inode->mtime);
    timeinfo = localtime(&inode->ctime);
    pr_debug(
        " Change : %4d-%02d-%02d %2d:%2d:%2d (%d)\n", timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday,
        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, inode->ctime);
    timeinfo = localtime(&inode->dtime);
    pr_debug(
        " Delete : %4d-%02d-%02d %2d:%2d:%2d (%d)\n", timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday,
        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, inode->dtime);
    pr_debug("  Links : %2u Flags : %d\n", inode->links_count, inode->flags);
    pr_debug(" Blocks : [ ");
    for (int i = 0; i < EXT2_DIRECT_BLOCKS; ++i) {
        if (inode->data.blocks.dir_blocks[i]) {
            pr_debug("%u ", inode->data.blocks.dir_blocks[i]);
        }
    }
    pr_debug("]\n");
    pr_debug("IBlocks : %u", inode->data.blocks.indir_block);
    if (inode->data.blocks.indir_block) {
        pr_debug(" [ ");
        for (uint32_t it = EXT2_DIRECT_BLOCKS; it < (inode->size / fs->block_size); ++it) {
            pr_debug("%u ", ext2_get_real_block_index(fs, inode, it));
        }
        pr_debug("]");
    }
    pr_debug("\n", inode->data.blocks.indir_block);

    pr_debug("DBlocks : %u\n", inode->data.blocks.doubly_indir_block);
    pr_debug("TBlocks : %u\n", inode->data.blocks.trebly_indir_block);
    pr_debug("Symlink : %s\n", inode->data.symlink);
    pr_debug("Generation : %u file_acl : %u dir_acl : %u\n", inode->generation, inode->file_acl, inode->dir_acl);
    (void)timeinfo;
}

/// @brief Dumps on debugging output the dirent.
/// @param dirent the object to dump.
void ext2_dump_dirent(ext2_dirent_t *dirent)
{
    pr_debug(
        "Inode: %4u Rec. Len.: %4u Name Len.: %4u Type: %4s Name: %s\n", dirent->inode, dirent->rec_len,
        dirent->name_len, ext2_file_type_to_string(dirent->file_type), dirent->name);
}

/// @brief Dumps on debugging output the BGDT.
/// @param fs the filesystem of which we print the BGDT.
void ext2_dump_bgdt(ext2_filesystem_t *fs)
{
    // Allocate the cache.
    uint8_t *cache = ext2_alloc_cache(fs);
    for (uint32_t i = 0; i < fs->block_groups_count; ++i) {
        // Get the pointer to the current group descriptor.
        ext2_group_descriptor_t *gd = &(fs->block_groups[i]);
        pr_debug("Block Group Descriptor [%u] @ %u:\n", i, fs->bgdt_start_block + i * fs->superblock.blocks_per_group);
        pr_debug("    block_bitmap : %u\n", gd->block_bitmap);
        pr_debug("    inode_bitmap : %u\n", gd->inode_bitmap);
        pr_debug("    inode_table  : %u\n", gd->inode_table);
        pr_debug("    Used Dirs    : %u\n", gd->used_dirs_count);
        pr_debug("    Free Blocks  : %4u of %u\n", gd->free_blocks_count, fs->superblock.blocks_per_group);
        pr_debug("    Free Inodes  : %4u of %u\n", gd->free_inodes_count, fs->superblock.inodes_per_group);
        // Dump the block bitmap.
        ext2_read_block(fs, gd->block_bitmap, cache);
        pr_debug("    Block Bitmap at %u\n", gd->block_bitmap);
        for (uint32_t j = 0; j < fs->block_size; ++j) {
            if ((j % 8) == 0) {
                pr_debug("        Block index: %4u, Bitmap: %s\n", j / 8, dec_to_binary(cache[j / 8], 8));
            }
            if (!ext2_bitmap_check(cache, j)) {
                pr_debug(
                    "    First free block in group is in block %u, the linear "
                    "index is %u\n",
                    j / 8, j);
                break;
            }
        }
        // Dump the block bitmap.
        ext2_read_block(fs, gd->inode_bitmap, cache);
        pr_debug("    Inode Bitmap at %d\n", gd->inode_bitmap);
        for (uint32_t j = 0; j < fs->block_size; ++j) {
            if ((j % 8) == 0) {
                pr_debug("        Block index: %4d, Bitmap: %s\n", j / 8, dec_to_binary(cache[j / 8], 8));
            }
            if (!ext2_bitmap_check(cache, j)) {
                pr_debug(
                    "    First free block in group is in block %d, the linear "
                    "index is %d\n",
                    j / 8, j);
                break;
            }
        }
    }
    ext2_dealloc_cache(cache);
}

/// @brief Dumps on debugging output the filesystem.
/// @param fs the object to dump.
void ext2_dump_filesystem(ext2_filesystem_t *fs)
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

/// @brief Dumps the directory entries inside the parent directory.
/// @param fs A pointer to the filesystem structure.
/// @param cache A pointer to a memory cache used for EXT2 operations.
/// @param parent_inode A pointer to the parent inode whose directory entries
/// are to be dumped.
void ext2_dump_direntries(ext2_filesystem_t *fs, uint8_t *cache, ext2_inode_t *parent_inode)
{
    /// Initialize the directory entry iterator for the parent directory
    ext2_direntry_iterator_t it = ext2_direntry_iterator_begin(fs, cache, parent_inode);

    /// Iterate through all directory entries in the parent directory
    for (; ext2_direntry_iterator_valid(&it); ext2_direntry_iterator_next(&it)) {
        /// Dump debug information for the directory entry's block index and offset
        pr_debug("    [Block: %2u, Offset: %4u] ", it.block_index, it.block_offset);

        /// Dump detailed information about the current directory entry
        ext2_dump_dirent(it.direntry);
    }
}
