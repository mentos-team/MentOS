/// @file t_nospace.c
/// @brief Regression tests for what the filesystem does when it runs out of
/// space: #264, mkdir must fail and leave nothing behind, and #303, a write
/// that cannot allocate its block must not report success.
/// @details ext2_mkdir allocates an inode, adds the entry to the parent and
/// only then allocates the data block that holds `.` and `..`. When that
/// last allocation failed the function returned 0, so user space saw a
/// successful mkdir for a directory with no data block and no entries at
/// all. Every failure after the inode allocation also reported -ENOENT
/// regardless of the cause, and none of them freed the inode that
/// ext2_allocate_inode had already persisted, so each attempt stranded an
/// allocated-but-unreferenced inode and left the group counters drifted.
///
/// The filesystem is filled with small files, each of them below the twelve
/// direct blocks of an inode, so the fill needs no index blocks: that keeps
/// it clear of #301 (the block-pointer boundary defect) and #302 (index
/// blocks leaked on free). The fill is driven by the free-block count from
/// statfs rather than by the result of write, because a write that cannot
/// allocate its block still reports success (#303). When mkdir wrongly
/// reports success, the directory it claims to have created is left in
/// place: removing it walks a zeroed directory block and never returns
/// (#304).
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

/// Where the checks operate.
#define BASE_DIR "/home/user"

/// The directory the test tries to create with no space left.
#define DIR_PATH BASE_DIR "/t_nospace.d"

/// The file used to check what a write does with no space left. It is
/// created before the fill, so that the check does not depend on being able
/// to create a file on a full filesystem.
#define TARGET_PATH BASE_DIR "/t_nospace.target"

/// Size of a single write, one block of the image.
#define CHUNK 4096

/// Blocks per fill file, the direct blocks of an inode.
#define BLOCKS_PER_FILE 12

/// Upper bound on the number of fill files.
#define MAX_FILES 2000

/// Buffer used for the fill writes.
static char buffer[CHUNK];

/// @brief Builds the path of a fill file.
/// @param path the destination buffer.
/// @param index the index of the fill file.
static void __fill_path(char *path, int index) { sprintf(path, BASE_DIR "/t_nospace.%04d", index); }

/// @brief Reads the free counters of the filesystem.
/// @param blocks where the free block count is stored, may be NULL.
/// @param inodes where the free inode count is stored, may be NULL.
/// @return 0 on success, -1 on failure.
static int __read_free(unsigned long *blocks, unsigned long *inodes)
{
    statfs_t buf;
    if (statfs(BASE_DIR, &buf) < 0) {
        syslog(LOG_ERR, "[t_nospace] statfs: %s", strerror(errno));
        return -1;
    }
    if (blocks) {
        *blocks = (unsigned long)buf.f_bfree;
    }
    if (inodes) {
        *inodes = (unsigned long)buf.f_ffree;
    }
    return 0;
}

/// @brief Consumes every free block of the filesystem.
/// @param files where the number of created files is stored.
/// @return 0 when no free block is left, -1 on failure.
static int __fill_filesystem(int *files)
{
    char path[64];
    unsigned long blocks = 0;
    memset(buffer, 'f', sizeof(buffer));
    *files = 0;
    for (int index = 0; index < MAX_FILES; ++index) {
        if (__read_free(&blocks, NULL) < 0) {
            return -1;
        }
        if (blocks == 0) {
            return 0;
        }
        __fill_path(path, index);
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            syslog(LOG_ERR, "[t_nospace] open %s: %s", path, strerror(errno));
            return -1;
        }
        ++(*files);
        for (int block = 0; block < BLOCKS_PER_FILE; ++block) {
            if (write(fd, buffer, sizeof(buffer)) != (ssize_t)sizeof(buffer)) {
                break;
            }
        }
        close(fd);
    }
    syslog(LOG_ERR, "[t_nospace] the filesystem still has free blocks after %d files", MAX_FILES);
    return -1;
}

/// @brief Removes the fill files.
/// @param files the number of fill files created.
static void __remove_fill(int files)
{
    char path[64];
    for (int index = 0; index < files; ++index) {
        __fill_path(path, index);
        unlink(path);
    }
}

/// @brief Creates the file used by the write check, with one block of data.
/// @return 0 on success, -1 on failure.
static int __prepare_target(void)
{
    memset(buffer, 'a', sizeof(buffer));
    int fd = open(TARGET_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_nospace] open %s: %s", TARGET_PATH, strerror(errno));
        return -1;
    }
    ssize_t written = write(fd, buffer, sizeof(buffer));
    close(fd);
    if (written != (ssize_t)sizeof(buffer)) {
        syslog(LOG_ERR, "[t_nospace] preparing the target wrote %zd bytes", written);
        return -1;
    }
    return 0;
}

/// @brief Appends a block to the target file with no space left.
/// @details The block cannot be allocated, so the write has to say so: it
///          either fails with ENOSPC or reports fewer bytes than asked. It
///          used to return the full count and drop the data, leaving the file
///          claiming a size it could not back (#303).
/// @return 0 on success, -1 on failure.
static int check_write_with_no_space(void)
{
    int failures = 0;
    memset(buffer, 'b', sizeof(buffer));
    int fd = open(TARGET_PATH, O_WRONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_nospace] open %s for appending: %s", TARGET_PATH, strerror(errno));
        return -1;
    }
    // Append past the end of the file, so the write needs a block that the
    // file does not already own.
    if (lseek(fd, 2 * CHUNK, SEEK_SET) != (2 * CHUNK)) {
        syslog(LOG_ERR, "[t_nospace] lseek on the target: %s", strerror(errno));
        close(fd);
        return -1;
    }
    errno           = 0;
    ssize_t written = write(fd, buffer, CHUNK);
    close(fd);
    if (written == CHUNK) {
        syslog(LOG_ERR, "[t_nospace] the write reported %zd bytes with no free block left", written);
        ++failures;
    } else if ((written < 0) && (errno != ENOSPC)) {
        syslog(LOG_ERR, "[t_nospace] the write failed with %s, expected ENOSPC", strerror(errno));
        ++failures;
    }

    // Whatever the write reported, the file must not claim bytes it does not
    // hold: the size covers the first block plus whatever was stored.
    stat_t st;
    if (stat(TARGET_PATH, &st) < 0) {
        syslog(LOG_ERR, "[t_nospace] stat %s: %s", TARGET_PATH, strerror(errno));
        return -1;
    }
    unsigned long expected = ((written > 0) ? (2 * CHUNK + (unsigned long)written) : CHUNK);
    if ((unsigned long)st.st_size != expected) {
        syslog(
            LOG_ERR, "[t_nospace] the target is %lu bytes, expected %lu after a write that reported %zd",
            (unsigned long)st.st_size, expected, written);
        ++failures;
    }
    return (failures == 0) ? 0 : -1;
}

int main(void)
{
    unsigned long blocks_before = 0;
    unsigned long inodes_before = 0;
    unsigned long blocks_full   = 0;
    unsigned long inodes_full   = 0;
    unsigned long inodes_after  = 0;
    int files                   = 0;
    int failures                = 0;

    if (__read_free(&blocks_before, &inodes_before) < 0) {
        return EXIT_FAILURE;
    }
    syslog(LOG_INFO, "[t_nospace] before: %lu free blocks, %lu free inodes", blocks_before, inodes_before);

    if (__prepare_target() < 0) {
        return EXIT_FAILURE;
    }
    if (__fill_filesystem(&files) < 0) {
        __remove_fill(files);
        unlink(TARGET_PATH);
        return EXIT_FAILURE;
    }
    if (__read_free(&blocks_full, &inodes_full) < 0) {
        __remove_fill(files);
        return EXIT_FAILURE;
    }
    syslog(
        LOG_INFO, "[t_nospace] full after %d files: %lu free blocks, %lu free inodes", files, blocks_full,
        inodes_full);

    // With no room for the directory data block, mkdir must fail, and it must
    // say why: every cause used to be reported as ENOENT.
    errno                            = 0;
    int ret                          = mkdir(DIR_PATH, 0755);
    unsigned long inodes_after_mkdir = 0;
    if (__read_free(NULL, &inodes_after_mkdir) < 0) {
        __remove_fill(files);
        return EXIT_FAILURE;
    }
    if (ret == 0) {
        syslog(LOG_ERR, "[t_nospace] mkdir succeeded with a full filesystem");
        ++failures;
        // The directory it claims to have created has no entries at all, and
        // it is not removed here: rmdir walks its zeroed block and never
        // comes back (#304).
        int fd = open(DIR_PATH, O_RDONLY | O_DIRECTORY, 0);
        syslog(
            LOG_ERR, "[t_nospace] open of the claimed directory returned %d (%s)", fd,
            (fd < 0) ? strerror(errno) : "no error");
        if (fd >= 0) {
            close(fd);
        }
    } else if (errno != ENOSPC) {
        syslog(LOG_ERR, "[t_nospace] mkdir failed with %s, expected ENOSPC", strerror(errno));
        ++failures;
    }

    // A write that cannot allocate its block must not report success.
    if (check_write_with_no_space() < 0) {
        ++failures;
    }

    // The failed mkdir must have given back the inode it had allocated. The
    // count is compared against the one taken right before the call, so that
    // whatever the fill itself consumed or stranded stays out of the check.
    if (inodes_after_mkdir != inodes_full) {
        syslog(
            LOG_ERR, "[t_nospace] free inodes went from %lu to %lu across the failed mkdir: it stranded one",
            inodes_full, inodes_after_mkdir);
        ++failures;
    }

    // Give the space back. Free blocks and inodes are only reported here:
    // the parent directory keeps the blocks it grew to hold the fill entries,
    // and ext2 never shrinks a directory.
    __remove_fill(files);
    unlink(TARGET_PATH);
    if (__read_free(NULL, &inodes_after) < 0) {
        return EXIT_FAILURE;
    }
    syslog(
        LOG_INFO, "[t_nospace] after the fill was removed: %lu free inodes (%lu before the test)", inodes_after,
        inodes_before);

    if (failures == 0) {
        syslog(LOG_INFO, "[t_nospace] mkdir and write both reported the full filesystem");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_nospace] %d FAILURES", failures);
    return EXIT_FAILURE;
}
