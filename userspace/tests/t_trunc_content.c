/// @file t_trunc_content.c
/// @brief Regression test for #315: truncating a file larger than one block
/// must not write anything but zeroes, and must not make the file grow.
/// @details ext2_clean_inode_content walked the file one block at a time with
/// a one-block buffer, but computed the length of each write as
/// `max(min(offset + cache_size, inode->size), 0)`, which is the offset of the
/// end of the window rather than its length. The first iteration was right by
/// accident, since its offset is 0; from the second on it asked
/// ext2_write_inode_data for more bytes than the buffer held, so the driver
/// copied whatever followed the one-block slab object into the file, and the
/// last iteration wrote past the end of the file and grew it.
///
/// The file therefore has to be larger than one block for the defect to show:
/// t_write_read already truncates a file, but a 6-byte one, so its single
/// iteration never leaves the range where the arithmetic happens to work.
///
/// Both consequences are checked. The growth is the deterministic one and does
/// not depend on what the kernel heap happens to hold; the non-zero bytes are
/// the reason the defect matters, because they are kernel memory that a
/// program can read back.
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

/// The file used by the check.
#define PATH "/home/user/t_trunc_content.bin"

/// Size of a filesystem block in the image.
#define BLOCK_SIZE 4096

/// Size of the file. Three blocks and a bit, so the loop inside the driver
/// runs four times and the last one lands past the end of the file.
#define FILE_SIZE ((3 * BLOCK_SIZE) + 100)

/// Byte written everywhere, so that anything left over is visible.
#define FILLER 0xAB

/// Buffer used for writing and for reading back, one block at a time.
static char buffer[BLOCK_SIZE];

/// @brief Creates the file and fills it with FILLER.
/// @return 0 on success, -1 on failure.
static int __create_and_fill(void)
{
    int fd = open(PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

/// @brief Counts the bytes of the file that are not zero.
/// @param nonzero where the count is stored.
/// @param first where the offset of the first non-zero byte is stored.
/// @return 0 on success, -1 on failure.
static int __count_nonzero(unsigned *nonzero, unsigned *first)
{
    int fd = open(PATH, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_trunc_content] open for reading: %s", strerror(errno));
        return -1;
    }
    *nonzero      = 0;
    *first        = 0;
    unsigned seen = 0;
    ssize_t bytes;
    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes; ++i) {
            if (buffer[i] != 0) {
                if (*nonzero == 0) {
                    *first = seen + (unsigned)i;
                }
                ++(*nonzero);
            }
        }
        seen += (unsigned)bytes;
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
    unsigned before  = 0;
    unsigned after   = 0;
    unsigned nonzero = 0;
    unsigned first   = 0;
    int failures     = 0;

    if (__create_and_fill() < 0) {
        return EXIT_FAILURE;
    }
    if (__size_of(&before) < 0) {
        unlink(PATH);
        return EXIT_FAILURE;
    }
    if (before != FILE_SIZE) {
        syslog(LOG_ERR, "[t_trunc_content] the file is %u bytes, expected %u", before, (unsigned)FILE_SIZE);
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

    // Truncating a file cannot make it longer. #320 tracks the fact that it
    // does not shorten it either, which is why this is not an equality.
    if (__size_of(&after) < 0) {
        ++failures;
    } else if (after > before) {
        syslog(LOG_ERR, "[t_trunc_content] truncating grew the file from %u bytes to %u", before, after);
        ++failures;
    }

    // Whatever length is left, none of it may be anything but zero: a non-zero
    // byte here came from kernel memory past the one-block buffer.
    if (__count_nonzero(&nonzero, &first) < 0) {
        ++failures;
    } else if (nonzero != 0) {
        syslog(
            LOG_ERR, "[t_trunc_content] %u bytes of the truncated file are not zero, the first at offset %u", nonzero,
            first);
        ++failures;
    }

    unlink(PATH);

    if (failures == 0) {
        syslog(LOG_INFO, "[t_trunc_content] truncating %u bytes left only zeroes and did not grow the file", before);
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_trunc_content] %d FAILURES", failures);
    return EXIT_FAILURE;
}
