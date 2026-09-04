/// @file t_dir_entries.c
/// @brief Regression test for #309: a directory that grows past one block must
/// keep the entries it already held.
/// @details When the entries no longer fit the first block,
/// ext2_create_new_direntry allocated the new block at index 0, replacing the
/// mapping of the block that held every existing name, and then set the
/// directory size to a single block. The names in the first block were lost,
/// the block itself was leaked, and the new entry sat at a block index the
/// size did not cover, so no directory walk could reach it. Creating a few
/// hundred files in one directory therefore kept only the most recent ones,
/// with no error reported anywhere.
///
/// The files are empty, so no data block is involved and the filesystem never
/// runs out of space: the only thing under test is how the directory grows.
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

/// The directory holding the entries, created by the test.
#define DIR_PATH "/home/user/t_dir_entries.d"

/// How many files to create. With 4096-byte blocks and these names the first
/// block holds a little under two hundred entries, so this spans three.
#define FILE_COUNT 400

/// @brief Builds the path of one of the files.
/// @param path the destination buffer.
/// @param index the index of the file.
static void __file_path(char *path, int index) { sprintf(path, DIR_PATH "/f.%04d", index); }

/// @brief Creates an empty file.
/// @param path the file to create.
/// @return 0 on success, -1 on failure.
static int __create(const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_dir_entries] open %s: %s", path, strerror(errno));
        return -1;
    }
    close(fd);
    return 0;
}

/// @brief Counts how many of the created files can be reached by name.
/// @param count how many files were created.
/// @return the number of files found.
static int __count_by_name(int count)
{
    char path[64];
    stat_t st;
    int found = 0;
    for (int index = 0; index < count; ++index) {
        __file_path(path, index);
        if (stat(path, &st) == 0) {
            ++found;
        }
    }
    return found;
}

/// @brief Counts the entries the directory lists, excluding `.` and `..`.
/// @return the number of entries listed, or -1 on failure.
static int __count_by_listing(void)
{
    static dirent_t entries[16];
    int fd = open(DIR_PATH, O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_dir_entries] open %s: %s", DIR_PATH, strerror(errno));
        return -1;
    }
    // The records come back one after the other, each the size of a dirent_t,
    // which is how ls walks them.
    int listed = 0;
    ssize_t bytes;
    while ((bytes = getdents(fd, entries, sizeof(entries))) > 0) {
        for (size_t i = 0; i < (size_t)bytes / sizeof(dirent_t); ++i) {
            if ((strcmp(entries[i].d_name, ".") != 0) && (strcmp(entries[i].d_name, "..") != 0)) {
                ++listed;
            }
        }
    }
    close(fd);
    if (bytes < 0) {
        syslog(LOG_ERR, "[t_dir_entries] getdents: %s", strerror(errno));
        return -1;
    }
    return listed;
}

/// @brief Removes the files and the directory.
/// @param count how many files were created.
static void __cleanup(int count)
{
    char path[64];
    for (int index = 0; index < count; ++index) {
        __file_path(path, index);
        unlink(path);
    }
    rmdir(DIR_PATH);
}

int main(void)
{
    char path[64];
    int failures = 0;
    int created  = 0;

    if ((mkdir(DIR_PATH, 0755) < 0) && (errno != EEXIST)) {
        syslog(LOG_ERR, "[t_dir_entries] mkdir %s: %s", DIR_PATH, strerror(errno));
        return EXIT_FAILURE;
    }

    // The first name is the one that used to disappear: it lives in the block
    // that the directory replaced when it grew.
    for (int index = 0; index < FILE_COUNT; ++index) {
        __file_path(path, index);
        if (__create(path) < 0) {
            break;
        }
        ++created;
    }
    if (created != FILE_COUNT) {
        syslog(LOG_ERR, "[t_dir_entries] created %d files out of %d", created, FILE_COUNT);
        ++failures;
    }

    // Every file created must still be reachable by name.
    int found = __count_by_name(created);
    if (found != created) {
        syslog(LOG_ERR, "[t_dir_entries] %d of the %d files created can be found by name", found, created);
        ++failures;
    }

    // And the directory must list them all, which needs its size to cover
    // every block it uses.
    int listed = __count_by_listing();
    if (listed < 0) {
        ++failures;
    } else if (listed != created) {
        syslog(LOG_ERR, "[t_dir_entries] the directory lists %d entries, %d were created", listed, created);
        ++failures;
    }

    __cleanup(created);

    if (failures == 0) {
        syslog(LOG_INFO, "[t_dir_entries] the directory kept all %d entries across its blocks", created);
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_dir_entries] %d FAILURES", failures);
    return EXIT_FAILURE;
}
