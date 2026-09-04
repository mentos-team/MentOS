/// @file t_trunc_content.c
/// @brief Regression test for #315 and #320: a truncating open must leave the
/// file empty, and must give its blocks back.
/// @details Two defects met on this path.
///
/// #315: ext2_clean_inode_content walked the file one block at a time with a
/// one-block buffer, but computed the length of each write as
/// `max(min(offset + cache_size, inode->size), 0)`, which is the offset of the
/// end of the window rather than its length. The first iteration was right by
/// accident, since its offset is 0; from the second on it asked
/// ext2_write_inode_data for more bytes than the buffer held, so the driver
/// copied whatever followed the one-block slab object into the file, and the
/// last iteration wrote past the end of the file and grew it.
///
/// #320: nothing set the size to zero. The content was overwritten with
/// zeroes and the length stayed whatever it was, so `stat` kept reporting the
/// old size, reading returned that many zero bytes instead of end-of-file,
/// and the blocks stayed allocated and attributed to the file.
///
/// The file therefore has to be larger than one block for either defect to
/// show: t_write_read already truncates a file, but a 6-byte one, so its
/// single iteration never leaves the range where the #315 arithmetic happens
/// to work, and one block is too little to see space come back.
///
/// The file is created empty before the measurement starts, so that the
/// directory entry already exists and a block the directory needs to grow
/// cannot be counted against the file. Four blocks keeps it inside the twelve
/// direct pointers of the inode, so no index block is involved either and the
/// expected counts are exact.
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
#define PATH "/home/user/t_trunc_content.bin"

/// Size of a filesystem block in the image.
#define BLOCK_SIZE 4096

/// Size of the file. Three blocks and a bit, so the loop inside the driver
/// runs four times and the last one lands past the end of the file.
#define FILE_SIZE ((3 * BLOCK_SIZE) + 100)

/// How many blocks FILE_SIZE occupies: three whole ones and the remainder.
#define FILE_BLOCKS 4

/// Byte written everywhere, so that anything left over is visible.
#define FILLER 0xAB

/// Buffer used for writing and for reading back, one block at a time.
static char buffer[BLOCK_SIZE];

/// @brief Creates the file, empty, so the directory entry exists before the
///        measurement starts.
/// @return 0 on success, -1 on failure.
static int __create_empty(void)
{
    int fd = open(PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_trunc_content] open for creating: %s", strerror(errno));
        return -1;
    }
    close(fd);
    return 0;
}

/// @brief Fills the file with FILLER.
/// @return 0 on success, -1 on failure.
static int __fill(void)
{
    int fd = open(PATH, O_WRONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_trunc_content] open for filling: %s", strerror(errno));
        return -1;
    }
    memset(buffer, FILLER, sizeof(buffer));
    for (size_t done = 0; done < FILE_SIZE;) {
        size_t chunk    = ((FILE_SIZE - done) < sizeof(buffer)) ? (FILE_SIZE - done) : sizeof(buffer);
        ssize_t written = write(fd, buffer, chunk);
        if (written != (ssize_t)chunk) {
            syslog(LOG_ERR, "[t_trunc_content] write at %u returned %zd: %s", (unsigned)done, written, strerror(errno));
            close(fd);
            return -1;
        }
        done += chunk;
    }
    close(fd);
    return 0;
}

/// @brief Reads the size of the file.
/// @param size where the size is stored.
/// @return 0 on success, -1 on failure.
static int __size_of(unsigned *size)
{
    stat_t st;
    if (stat(PATH, &st) < 0) {
        syslog(LOG_ERR, "[t_trunc_content] stat: %s", strerror(errno));
        return -1;
    }
    *size = (unsigned)st.st_size;
    return 0;
}

/// @brief Reads the number of free blocks of the filesystem.
/// @param blocks where the count is stored.
/// @return 0 on success, -1 on failure.
static int __free_blocks(unsigned long *blocks)
{
    statfs_t buf;
    if (statfs("/home/user", &buf) < 0) {
        syslog(LOG_ERR, "[t_trunc_content] statfs: %s", strerror(errno));
        return -1;
    }
    *blocks = (unsigned long)buf.f_bfree;
    return 0;
}

/// @brief Reads the whole file, counting what it holds.
/// @param total where the number of bytes read before end-of-file is stored.
/// @param nonzero where the count of bytes that are not zero is stored.
/// @param first where the offset of the first non-zero byte is stored.
/// @return 0 on success, -1 on failure.
static int __read_all(unsigned *total, unsigned *nonzero, unsigned *first)
{
    int fd = open(PATH, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_trunc_content] open for reading: %s", strerror(errno));
        return -1;
    }
    *total   = 0;
    *nonzero = 0;
    *first   = 0;
    ssize_t bytes;
    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes; ++i) {
            if (buffer[i] != 0) {
                if (*nonzero == 0) {
                    *first = *total + (unsigned)i;
                }
                ++(*nonzero);
            }
        }
        *total += (unsigned)bytes;
    }
    close(fd);
    if (bytes < 0) {
        syslog(LOG_ERR, "[t_trunc_content] read: %s", strerror(errno));
        return -1;
    }
    return 0;
}

int main(void)
{
    unsigned long free_empty  = 0;
    unsigned long free_filled = 0;
    unsigned long free_after  = 0;
    unsigned size             = 0;
    unsigned total            = 0;
    unsigned nonzero          = 0;
    unsigned first            = 0;
    int failures              = 0;

    if ((__create_empty() < 0) || (__free_blocks(&free_empty) < 0) || (__fill() < 0) ||
        (__free_blocks(&free_filled) < 0)) {
        unlink(PATH);
        return EXIT_FAILURE;
    }

    // The starting state has to be the one the test assumes, otherwise every
    // check below can pass without meaning anything.
    if (__size_of(&size) < 0) {
        unlink(PATH);
        return EXIT_FAILURE;
    }
    if (size != FILE_SIZE) {
        syslog(LOG_ERR, "[t_trunc_content] the filled file is %u bytes, expected %u", size, (unsigned)FILE_SIZE);
        unlink(PATH);
        return EXIT_FAILURE;
    }
    if ((free_empty - free_filled) != FILE_BLOCKS) {
        syslog(
            LOG_ERR, "[t_trunc_content] filling the file took %lu blocks, expected %u", free_empty - free_filled,
            (unsigned)FILE_BLOCKS);
        unlink(PATH);
        return EXIT_FAILURE;
    }

    // Truncating happens while the file is being opened, so opening and
    // closing it is the whole operation under test.
    int fd = open(PATH, O_WRONLY | O_TRUNC, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_trunc_content] open with O_TRUNC: %s", strerror(errno));
        unlink(PATH);
        return EXIT_FAILURE;
    }
    close(fd);

    // A truncating open leaves the file with no content at all.
    if (__size_of(&size) < 0) {
        ++failures;
    } else if (size != 0) {
        syslog(LOG_ERR, "[t_trunc_content] the truncated file is %u bytes, expected 0", size);
        ++failures;
    }

    // Reading it therefore reports end-of-file straight away, and nothing it
    // returns may be anything but zero: a non-zero byte here came from kernel
    // memory past the one-block buffer.
    if (__read_all(&total, &nonzero, &first) < 0) {
        ++failures;
    } else {
        if (total != 0) {
            syslog(LOG_ERR, "[t_trunc_content] reading the truncated file returned %u bytes, expected 0", total);
            ++failures;
        }
        if (nonzero != 0) {
            syslog(
                LOG_ERR, "[t_trunc_content] %u bytes of the truncated file are not zero, the first at offset %u",
                nonzero, first);
            ++failures;
        }
    }

    // And the space it held goes back to the filesystem.
    if (__free_blocks(&free_after) < 0) {
        ++failures;
    } else if (free_after != free_empty) {
        syslog(
            LOG_ERR, "[t_trunc_content] truncating gave back %lu of the %u blocks the file held",
            free_after - free_filled, (unsigned)FILE_BLOCKS);
        ++failures;
    }

    unlink(PATH);

    if (failures == 0) {
        syslog(
            LOG_INFO, "[t_trunc_content] truncating %u bytes left an empty file and returned %u blocks",
            (unsigned)FILE_SIZE, (unsigned)FILE_BLOCKS);
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_trunc_content] %d FAILURES", failures);
    return EXIT_FAILURE;
}
