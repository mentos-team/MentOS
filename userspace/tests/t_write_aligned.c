/// @file t_write_aligned.c
/// @brief Regression test for #311: a write ending exactly on a block boundary
/// must not allocate a block past the data.
/// @details ext2_write_inode_data derived the last block of the transfer from
/// `end_offset / block_size`, which for a write ending on a boundary names the
/// block *after* the last one holding data, and leaves `end_size` at zero. The
/// loop then ran one extra iteration whose copy length, `end_size - 1 + 1` in
/// unsigned arithmetic, wrapped to zero: no bytes were copied, but the block
/// was allocated and written all the same. Every aligned write therefore took
/// one block more than it needed, permanently attributed to the file.
///
/// The read path had the same defect and #242 fixed it there, with a comment
/// naming this exact case; the write path was left as it was, which is what
/// this test pins down.
///
/// The file is created before the measurement starts, so that the directory
/// entry already exists and the block the directory might need to grow cannot
/// be counted as one of the file's. Eight blocks keeps the file inside the
/// twelve direct pointers of the inode, so no index block is involved either
/// and the expected count is exact.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <syslog.h>
#include <unistd.h>

/// The file used by the checks.
#define PATH "/home/user/t_write_aligned.bin"

/// Size of a filesystem block in the image.
#define BLOCK_SIZE 4096

/// How many whole blocks to write. Eight stays within the twelve direct
/// pointers of an inode, so the file needs no index block.
#define BLOCKS 8

/// Buffer holding one block.
static char buffer[BLOCK_SIZE];

/// @brief Reads the number of free blocks of the filesystem.
/// @param blocks where the count is stored.
/// @return 0 on success, -1 on failure.
static int __free_blocks(unsigned long *blocks)
{
    statfs_t buf;
    if (statfs("/home/user", &buf) < 0) {
        syslog(LOG_ERR, "[t_write_aligned] statfs: %s", strerror(errno));
        return -1;
    }
    *blocks = (unsigned long)buf.f_bfree;
    return 0;
}

/// @brief Creates the file, so that the directory entry exists before the
///        measurement starts.
/// @return 0 on success, -1 on failure.
static int __create_empty(void)
{
    int fd = open(PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_write_aligned] open for creating: %s", strerror(errno));
        return -1;
    }
    close(fd);
    return 0;
}

/// @brief Writes whole blocks into the file, one block per call.
/// @param count how many blocks to write.
/// @return 0 on success, -1 on failure.
static int __write_blocks(unsigned count)
{
    int fd = open(PATH, O_WRONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_write_aligned] open for writing: %s", strerror(errno));
        return -1;
    }
    memset(buffer, 'A', sizeof(buffer));
    for (unsigned block = 0; block < count; ++block) {
        ssize_t written = write(fd, buffer, sizeof(buffer));
        if (written != (ssize_t)sizeof(buffer)) {
            syslog(LOG_ERR, "[t_write_aligned] write of block %u returned %zd: %s", block, written, strerror(errno));
            close(fd);
            return -1;
        }
    }
    close(fd);
    return 0;
}

/// @brief Checks that the file reads back what was written into it.
/// @param count how many blocks were written.
/// @return 0 when the content matches, -1 otherwise.
static int __check_content(unsigned count)
{
    int fd = open(PATH, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_write_aligned] open for reading: %s", strerror(errno));
        return -1;
    }
    for (unsigned block = 0; block < count; ++block) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes = read(fd, buffer, sizeof(buffer));
        if (bytes != (ssize_t)sizeof(buffer)) {
            syslog(LOG_ERR, "[t_write_aligned] read of block %u returned %zd: %s", block, bytes, strerror(errno));
            close(fd);
            return -1;
        }
        for (size_t i = 0; i < sizeof(buffer); ++i) {
            if (buffer[i] != 'A') {
                syslog(LOG_ERR, "[t_write_aligned] block %u differs at byte %u", block, (unsigned)i);
                close(fd);
                return -1;
            }
        }
    }
    close(fd);
    return 0;
}

int main(void)
{
    unsigned long before = 0;
    unsigned long after  = 0;
    int failures         = 0;

    if (__create_empty() < 0) {
        return EXIT_FAILURE;
    }
    if (__free_blocks(&before) < 0) {
        unlink(PATH);
        return EXIT_FAILURE;
    }
    if (__write_blocks(BLOCKS) < 0) {
        unlink(PATH);
        return EXIT_FAILURE;
    }
    if (__free_blocks(&after) < 0) {
        unlink(PATH);
        return EXIT_FAILURE;
    }

    // Writing whole blocks has to consume exactly that many blocks.
    unsigned long consumed = before - after;
    if (consumed != BLOCKS) {
        syslog(
            LOG_ERR, "[t_write_aligned] writing %u whole blocks consumed %lu blocks", (unsigned)BLOCKS, consumed);
        ++failures;
    }

    // And the size has to match, so that a wrong count cannot be explained
    // away by the file holding more than it was asked to.
    stat_t st;
    if (stat(PATH, &st) < 0) {
        syslog(LOG_ERR, "[t_write_aligned] stat: %s", strerror(errno));
        ++failures;
    } else if ((unsigned)st.st_size != (unsigned)(BLOCKS * BLOCK_SIZE)) {
        syslog(
            LOG_ERR, "[t_write_aligned] the file is %u bytes, expected %u", (unsigned)st.st_size,
            (unsigned)(BLOCKS * BLOCK_SIZE));
        ++failures;
    }

    // The data still has to be there: bounding the transfer must not drop the
    // last block of it.
    if (__check_content(BLOCKS) < 0) {
        ++failures;
    }

    unlink(PATH);

    if (failures == 0) {
        syslog(LOG_INFO, "[t_write_aligned] %u whole blocks took exactly %u blocks", (unsigned)BLOCKS, (unsigned)BLOCKS);
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_write_aligned] %d FAILURES", failures);
    return EXIT_FAILURE;
}
