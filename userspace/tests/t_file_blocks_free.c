/// @file t_file_blocks_free.c
/// @brief Regression test for #302: deleting a file must return every block it
/// owned, the ones holding its block pointers included.
/// @details ext2_free_inode walked the data blocks of the file through
/// ext2_get_real_block_index and freed those, and nothing freed the blocks
/// that hold the pointers themselves. Any file large enough to need a level of
/// indirection therefore leaked one block per level on deletion, and repeated
/// create-and-delete cycles shrank the filesystem with no file left to remove
/// to get the space back.
///
/// The file is written in chunks that do not end on a block boundary, so the
/// accounting is not confounded by #311, which allocates one block too many
/// for a write that ends exactly on one.
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

/// The file used by the check.
#define PATH "/home/user/t_file_blocks_free.bin"

/// Bytes written per call, deliberately not a multiple of the block size.
#define CHUNK 4000

/// How many chunks to write. Twenty-one chunks cover more than the twelve
/// direct blocks of an inode, so the file needs a single-indirect block.
#define CHUNKS 21

/// Buffer holding one chunk.
static char buffer[CHUNK];

/// @brief Reads the number of free blocks of the filesystem.
/// @param blocks where the count is stored.
/// @return 0 on success, -1 on failure.
static int __free_blocks(unsigned long *blocks)
{
    statfs_t buf;
    if (statfs("/home/user", &buf) < 0) {
        syslog(LOG_ERR, "[t_file_blocks_free] statfs: %s", strerror(errno));
        return -1;
    }
    *blocks = (unsigned long)buf.f_bfree;
    return 0;
}

int main(void)
{
    unsigned long before = 0;
    unsigned long during = 0;
    unsigned long after  = 0;

    if (__free_blocks(&before) < 0) {
        return EXIT_FAILURE;
    }

    memset(buffer, 'k', sizeof(buffer));
    int fd = open(PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_file_blocks_free] open: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    for (int chunk = 0; chunk < CHUNKS; ++chunk) {
        if (write(fd, buffer, sizeof(buffer)) != (ssize_t)sizeof(buffer)) {
            syslog(LOG_ERR, "[t_file_blocks_free] write of chunk %d: %s", chunk, strerror(errno));
            close(fd);
            unlink(PATH);
            return EXIT_FAILURE;
        }
    }
    close(fd);

    if (__free_blocks(&during) < 0) {
        unlink(PATH);
        return EXIT_FAILURE;
    }
    // The file has to be large enough to need a level of indirection,
    // otherwise the check would pass without exercising anything.
    if ((before - during) <= 12) {
        syslog(
            LOG_ERR, "[t_file_blocks_free] the file took only %lu blocks, too few to need an index block",
            before - during);
        unlink(PATH);
        return EXIT_FAILURE;
    }

    if (unlink(PATH) < 0) {
        syslog(LOG_ERR, "[t_file_blocks_free] unlink: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    if (__free_blocks(&after) < 0) {
        return EXIT_FAILURE;
    }

    if (after != before) {
        syslog(
            LOG_ERR, "[t_file_blocks_free] %lu blocks were taken and %lu given back: %lu are missing",
            before - during, after - during, before - after);
        return EXIT_FAILURE;
    }
    syslog(
        LOG_INFO, "[t_file_blocks_free] the %lu blocks of the file came back, index blocks included",
        before - during);
    return EXIT_SUCCESS;
}
