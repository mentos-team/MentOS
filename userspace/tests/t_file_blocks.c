/// @file t_file_blocks.c
/// @brief Regression test for #301: every block of a file must read back what
/// was written into it, including the ones at the boundaries between the
/// direct, indirect and doubly-indirect regions of the inode.
/// @details ext2_set_real_block_index used `<= 0` where
/// ext2_get_real_block_index used `< 0` to decide which region a block index
/// belongs to, so the two disagreed on the first index of each region. Block
/// 12 had its pointer stored into dir_blocks[12], one past the array, which
/// is the single-indirect pointer that follows it: the data block of block 12
/// then served as the file's index block. Block 1036 had its pointer stored
/// one element past the end of the index block, so it was lost and the block
/// read back as zeros. Both were silent, with write reporting success.
///
/// The blocks are written at their own offsets with lseek, so only the blocks
/// under test are allocated and the test stays fast: the interesting indices
/// are the last of each region and the first of the next.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

/// The file used by the checks.
#define PATH "/home/user/t_file_blocks.bin"

/// Size of a filesystem block in the image.
#define BLOCK_SIZE 4096

/// Number of block pointers in one block, with 4-byte pointers.
#define POINTERS_PER_BLOCK (BLOCK_SIZE / 4)

/// Number of direct block pointers in an inode.
#define DIRECT_BLOCKS 12

/// The block indices under test: around the end of the direct region and
/// around the end of the single-indirect region.
static const unsigned block_indices[] = {
    0,
    DIRECT_BLOCKS - 1,
    DIRECT_BLOCKS,
    DIRECT_BLOCKS + 1,
    DIRECT_BLOCKS + POINTERS_PER_BLOCK - 1,
    DIRECT_BLOCKS + POINTERS_PER_BLOCK,
    DIRECT_BLOCKS + POINTERS_PER_BLOCK + 1,
};

/// Number of block indices under test.
#define BLOCK_COUNT (sizeof(block_indices) / sizeof(block_indices[0]))

/// Buffer holding one block.
static char buffer[BLOCK_SIZE];

/// @brief Fills the buffer with the marker of the given block index.
/// @param index the block index to encode.
static void __fill_marker(unsigned index)
{
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = (char)(index & 0xFF);
    buffer[1] = (char)((index >> 8) & 0xFF);
    buffer[2] = (char)((index >> 16) & 0xFF);
    buffer[3] = (char)((index >> 24) & 0xFF);
    buffer[4] = 'M';
    buffer[5] = 'K';
}

/// @brief Checks that the buffer holds the marker of the given block index.
/// @param index the expected block index.
/// @return 0 when the marker matches, -1 otherwise.
static int __check_marker(unsigned index)
{
    unsigned found = (unsigned char)buffer[0];
    found |= ((unsigned)(unsigned char)buffer[1]) << 8;
    found |= ((unsigned)(unsigned char)buffer[2]) << 16;
    found |= ((unsigned)(unsigned char)buffer[3]) << 24;
    if ((buffer[4] != 'M') || (buffer[5] != 'K') || (found != index)) {
        return -1;
    }
    return 0;
}

/// @brief Writes a contiguous run of blocks and reads it back.
/// @param first the first block index of the run.
/// @param count how many blocks the run holds.
/// @return the number of blocks that did not read back correctly.
static int check_contiguous_run(unsigned first, unsigned count)
{
    int failures = 0;
    int fd       = open(PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_file_blocks] open for writing the run: %s", strerror(errno));
        return 1;
    }
    if (lseek(fd, (off_t)first * BLOCK_SIZE, SEEK_SET) < 0) {
        syslog(LOG_ERR, "[t_file_blocks] lseek to block %u: %s", first, strerror(errno));
        close(fd);
        return 1;
    }
    for (unsigned index = first; index < (first + count); ++index) {
        __fill_marker(index);
        if (write(fd, buffer, sizeof(buffer)) != (ssize_t)sizeof(buffer)) {
            syslog(LOG_ERR, "[t_file_blocks] write of block %u in the run: %s", index, strerror(errno));
            ++failures;
        }
    }
    close(fd);

    fd = open(PATH, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_file_blocks] open for reading the run: %s", strerror(errno));
        return failures + 1;
    }
    if (lseek(fd, (off_t)first * BLOCK_SIZE, SEEK_SET) < 0) {
        syslog(LOG_ERR, "[t_file_blocks] lseek to block %u: %s", first, strerror(errno));
        close(fd);
        return failures + 1;
    }
    for (unsigned index = first; index < (first + count); ++index) {
        memset(buffer, 0, sizeof(buffer));
        if (read(fd, buffer, sizeof(buffer)) != (ssize_t)sizeof(buffer)) {
            syslog(LOG_ERR, "[t_file_blocks] read of block %u in the run: %s", index, strerror(errno));
            ++failures;
            continue;
        }
        if (__check_marker(index) < 0) {
            syslog(LOG_ERR, "[t_file_blocks] block %u of the run does not hold what was written into it", index);
            ++failures;
        }
    }
    close(fd);
    unlink(PATH);
    return failures;
}

int main(void)
{
    int failures = 0;

    int fd = open(PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_file_blocks] open for writing: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    for (unsigned i = 0; i < BLOCK_COUNT; ++i) {
        unsigned index = block_indices[i];
        off_t offset   = (off_t)index * BLOCK_SIZE;
        if (lseek(fd, offset, SEEK_SET) != offset) {
            syslog(LOG_ERR, "[t_file_blocks] lseek to block %u: %s", index, strerror(errno));
            ++failures;
            continue;
        }
        __fill_marker(index);
        ssize_t written = write(fd, buffer, sizeof(buffer));
        if (written != (ssize_t)sizeof(buffer)) {
            syslog(LOG_ERR, "[t_file_blocks] write of block %u returned %zd: %s", index, written, strerror(errno));
            ++failures;
        }
    }
    close(fd);

    fd = open(PATH, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_file_blocks] open for reading: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    for (unsigned i = 0; i < BLOCK_COUNT; ++i) {
        unsigned index = block_indices[i];
        off_t offset   = (off_t)index * BLOCK_SIZE;
        if (lseek(fd, offset, SEEK_SET) != offset) {
            syslog(LOG_ERR, "[t_file_blocks] lseek to block %u: %s", index, strerror(errno));
            ++failures;
            continue;
        }
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes = read(fd, buffer, sizeof(buffer));
        if (bytes != (ssize_t)sizeof(buffer)) {
            syslog(LOG_ERR, "[t_file_blocks] read of block %u returned %zd: %s", index, bytes, strerror(errno));
            ++failures;
            continue;
        }
        if (__check_marker(index) < 0) {
            syslog(LOG_ERR, "[t_file_blocks] block %u does not hold what was written into it", index);
            ++failures;
        }
    }
    close(fd);
    unlink(PATH);

    // The defect showed up while allocating blocks in order: the pointer of
    // block 12 landed on the single-indirect pointer, and the block after it
    // then used block 12 as its index block. A contiguous run across that
    // boundary covers it.
    failures += check_contiguous_run(0, DIRECT_BLOCKS + 4);

    if (failures == 0) {
        syslog(LOG_INFO, "[t_file_blocks] every block read back what was written");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_file_blocks] %d FAILURES", failures);
    return EXIT_FAILURE;
}
