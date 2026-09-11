/// @file t_dir_block_boundary.c
/// @brief Boundary coverage for #360: a directory entry appended at the very
/// end of a block must fit inside the block.
/// @details ext2_append_new_direntry shrinks the last entry of a block to its
/// real length and puts the new entry right after it, but it measured the free
/// space from the start of that last entry instead of from its end. The check
/// therefore passed with `real_rec_len` bytes less room than it thought: the
/// new entry was placed with room for its 8-byte header alone, or less, and
/// its name was written past the end of the block buffer, over whatever the
/// slab allocator kept next to it. The name never reached the disk either, so
/// the entry described a name that was not there.
///
/// The test walks both shapes of the boundary, with 4096-byte blocks:
///   - short names, where the last record ends at offset 4088 and the name
///     went 7 bytes past the block;
///   - a long name after a long one, where the record ends at offset 4092 and
///     the name went 255 bytes past the block.
///
/// What this covers and what it does not: before the fix the write went out of
/// bounds in both cases, but it landed in a neighbouring buffer that kept the
/// bytes, so every lookup still found the name and this test passed. The
/// deterministic pre-fix failure was a kernel panic in the slab allocator,
/// once an unrelated change to the allocation pattern left that neighbouring
/// buffer on a free list. So this file is boundary coverage for the layout,
/// not a detector of the original corruption; the detector is the bound check
/// ext2_initialize_direntry now performs.
///
/// The files are empty, so no data block is involved and only the directory
/// layout is under test.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

/// The directory used for the short-name case.
#define DIR_PATH "/home/user/t_dir_block_boundary.d"

/// How many files the short-name case creates. With 4096-byte blocks, `.` and
/// `..` take 12 bytes each and a `f.NNNN` record takes 16, so the last record
/// of the first block ends at offset 24 + 254 * 16 = 4088, which is the case
/// that used to overflow; a few more files carry the directory into the next
/// block. With 1024-byte blocks the same happens at offset 1016.
#define FILE_COUNT 260

/// The directory used for the long-name case.
#define WIDE_DIR_PATH "/home/user/t_dir_block_boundary.wide"

/// How many short names bring the last record of the first block to the offset
/// where a long name passes the loose check but does not fit: with 4096-byte
/// blocks, 24 + 238 * 16 = 3832.
#define WIDE_FILL_COUNT 238

/// The length of the long names, chosen so their record is 260 bytes, four
/// more than the space the block really has left.
#define WIDE_NAME_LEN 250

/// @brief Builds the path of one of the short-named files.
/// @param path the destination buffer.
/// @param dir the directory holding the file.
/// @param index the index of the file.
static void __short_path(char *path, const char *dir, int index) { sprintf(path, "%s/f.%04d", dir, index); }

/// @brief Builds a path with a name of WIDE_NAME_LEN characters.
/// @param path the destination buffer.
/// @param last the last character of the name, to tell the two names apart.
static void __wide_path(char *path, char last)
{
    strcpy(path, WIDE_DIR_PATH "/");
    size_t offset = strlen(path);
    for (int i = 0; i < (WIDE_NAME_LEN - 1); ++i) {
        path[offset + i] = 'w';
    }
    path[offset + WIDE_NAME_LEN - 1] = last;
    path[offset + WIDE_NAME_LEN]     = 0;
}

/// @brief Creates an empty file.
/// @param path the file to create.
/// @return 0 on success, -1 on failure.
static int __create(const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_dir_block_boundary] open %s: %s", path, strerror(errno));
        return -1;
    }
    close(fd);
    return 0;
}

/// @brief Checks the short-name case: every name created stays reachable and
/// the directory lists exactly those names.
/// @return the number of failures found.
static int __check_short_boundary(void)
{
    static char seen[FILE_COUNT];
    static dirent_t entries[16];
    char path[64];
    stat_t st;
    int failures = 0;
    int created  = 0;

    if ((mkdir(DIR_PATH, 0755) < 0) && (errno != EEXIST)) {
        syslog(LOG_ERR, "[t_dir_block_boundary] mkdir %s: %s", DIR_PATH, strerror(errno));
        return 1;
    }
    for (int index = 0; index < FILE_COUNT; ++index) {
        __short_path(path, DIR_PATH, index);
        if (__create(path) < 0) {
            break;
        }
        ++created;
    }
    if (created != FILE_COUNT) {
        syslog(LOG_ERR, "[t_dir_block_boundary] created %d files out of %d", created, FILE_COUNT);
        ++failures;
    }

    // Every name created has to be reachable.
    for (int index = 0; index < created; ++index) {
        __short_path(path, DIR_PATH, index);
        if (stat(path, &st) < 0) {
            syslog(LOG_ERR, "[t_dir_block_boundary] stat %s: %s", path, strerror(errno));
            ++failures;
        }
    }

    // And the directory has to list those names, and only those.
    memset(seen, 0, sizeof(seen));
    int fd = open(DIR_PATH, O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_dir_block_boundary] open %s: %s", DIR_PATH, strerror(errno));
        ++failures;
    } else {
        ssize_t bytes;
        while ((bytes = getdents(fd, entries, sizeof(entries))) > 0) {
            for (size_t i = 0; i < (size_t)bytes / sizeof(dirent_t); ++i) {
                const char *name = entries[i].d_name;
                if ((strcmp(name, ".") == 0) || (strcmp(name, "..") == 0)) {
                    continue;
                }
                int index = -1;
                if ((strlen(name) == 6) && (name[0] == 'f') && (name[1] == '.')) {
                    index = atoi(name + 2);
                }
                if ((index < 0) || (index >= created)) {
                    syslog(LOG_ERR, "[t_dir_block_boundary] %s lists `%s`, which was never created", DIR_PATH, name);
                    ++failures;
                } else if (seen[index]) {
                    syslog(LOG_ERR, "[t_dir_block_boundary] %s lists `%s` more than once", DIR_PATH, name);
                    ++failures;
                } else {
                    seen[index] = 1;
                }
            }
        }
        close(fd);
        if (bytes < 0) {
            syslog(LOG_ERR, "[t_dir_block_boundary] getdents %s: %s", DIR_PATH, strerror(errno));
            ++failures;
        }
        for (int index = 0; index < created; ++index) {
            if (!seen[index]) {
                __short_path(path, DIR_PATH, index);
                syslog(LOG_ERR, "[t_dir_block_boundary] %s does not list `%s`", DIR_PATH, path);
                ++failures;
            }
        }
    }

    for (int index = 0; index < created; ++index) {
        __short_path(path, DIR_PATH, index);
        unlink(path);
    }
    rmdir(DIR_PATH);
    return failures;
}

/// @brief Checks the long-name case, the one that wrote the furthest past the
/// end of the block: a 250-character name arriving with four bytes of real
/// free space left in the block.
/// @return the number of failures found.
static int __check_wide_boundary(void)
{
    static dirent_t entries[16];
    char path[320];
    char expected[320];
    stat_t st;
    int failures = 0;

    if ((mkdir(WIDE_DIR_PATH, 0755) < 0) && (errno != EEXIST)) {
        syslog(LOG_ERR, "[t_dir_block_boundary] mkdir %s: %s", WIDE_DIR_PATH, strerror(errno));
        return 1;
    }
    // Fill the first block up to the offset where the long record lands.
    for (int index = 0; index < WIDE_FILL_COUNT; ++index) {
        __short_path(path, WIDE_DIR_PATH, index);
        if (__create(path) < 0) {
            ++failures;
            break;
        }
    }
    // The first long name becomes the last record of the block, and leaves
    // only four bytes of real free space behind it.
    __wide_path(path, 'a');
    if (__create(path) < 0) {
        ++failures;
    }
    // The second long name is the one that does not fit: it belongs in a new
    // block, and used to be written over the end of this one.
    __wide_path(path, 'b');
    if (__create(path) < 0) {
        ++failures;
    }
    // Both long names have to be reachable.
    __wide_path(path, 'a');
    if (stat(path, &st) < 0) {
        syslog(LOG_ERR, "[t_dir_block_boundary] the first long name is gone: %s", strerror(errno));
        ++failures;
    }
    __wide_path(path, 'b');
    if (stat(path, &st) < 0) {
        syslog(LOG_ERR, "[t_dir_block_boundary] the second long name is gone: %s", strerror(errno));
        ++failures;
    }
    // And the directory has to list every entry created. A record placed at
    // the very end of a block keeps its `rec_len` and `name_len` outside it,
    // so a walk of that block cannot reach what comes after.
    int listed = 0;
    int fd     = open(WIDE_DIR_PATH, O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_dir_block_boundary] open %s: %s", WIDE_DIR_PATH, strerror(errno));
        ++failures;
    } else {
        ssize_t bytes;
        while ((bytes = getdents(fd, entries, sizeof(entries))) > 0) {
            for (size_t i = 0; i < (size_t)bytes / sizeof(dirent_t); ++i) {
                const char *name = entries[i].d_name;
                if ((strcmp(name, ".") == 0) || (strcmp(name, "..") == 0)) {
                    continue;
                }
                ++listed;
                size_t length = strlen(name);
                if ((length == 6) && (name[0] == 'f') && (name[1] == '.')) {
                    int index = atoi(name + 2);
                    if ((index >= 0) && (index < WIDE_FILL_COUNT)) {
                        continue;
                    }
                }
                if (length == WIDE_NAME_LEN) {
                    __wide_path(expected, 'a');
                    if (strcmp(name, expected + strlen(WIDE_DIR_PATH "/")) == 0) {
                        continue;
                    }
                    __wide_path(expected, 'b');
                    if (strcmp(name, expected + strlen(WIDE_DIR_PATH "/")) == 0) {
                        continue;
                    }
                }
                syslog(
                    LOG_ERR, "[t_dir_block_boundary] %s lists a name of %u characters that was never created",
                    WIDE_DIR_PATH, (unsigned)length);
                ++failures;
            }
        }
        close(fd);
        if (bytes < 0) {
            syslog(LOG_ERR, "[t_dir_block_boundary] getdents %s: %s", WIDE_DIR_PATH, strerror(errno));
            ++failures;
        }
        if (listed != (WIDE_FILL_COUNT + 2)) {
            syslog(
                LOG_ERR, "[t_dir_block_boundary] %s lists %d entries, %d were created", WIDE_DIR_PATH, listed,
                WIDE_FILL_COUNT + 2);
            ++failures;
        }
    }

    for (int index = 0; index < WIDE_FILL_COUNT; ++index) {
        __short_path(path, WIDE_DIR_PATH, index);
        unlink(path);
    }
    __wide_path(path, 'a');
    unlink(path);
    __wide_path(path, 'b');
    unlink(path);
    rmdir(WIDE_DIR_PATH);
    return failures;
}

int main(void)
{
    int failures = __check_short_boundary() + __check_wide_boundary();

    if (failures == 0) {
        syslog(LOG_INFO, "[t_dir_block_boundary] both block boundaries kept their entries and their names");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_dir_block_boundary] %d FAILURES", failures);
    return EXIT_FAILURE;
}
