/// @file t_path_bounds.c
/// @brief Regression test for #284: path resolution must never silently
/// truncate a path or one of its components.
/// @details `tokenize()` used the capacity of the token buffer as the bound
/// for the offset into the *path*, so any component starting past byte 255 of
/// the path was dropped and a component longer than the buffer was truncated.
/// A request for a path then resolved to a different, shorter name: the file
/// named by a prefix of the request was opened or executed instead of the one
/// asked for. The checks below cover the three observable consequences:
///   - a file whose path is longer than 255 bytes can be created and read;
///   - a request for a name below an existing 255-byte-plus path fails
///     instead of resolving to that path;
///   - a component of exactly NAME_MAX characters or longer is rejected with
///     ENAMETOOLONG instead of being truncated to a name that exists.
/// The longest usable component, NAME_MAX - 1 characters, must keep working.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

/// Parent directory used by the checks. Every artifact lives in a directory
/// of its own, so the long names never share a directory with other tests.
#define BASE_DIR "/home/user/t_path_bounds.d"

/// Length of the directory component that pushes the path past 255 bytes.
#define LONG_DIR_LEN 250

/// @brief Fills the buffer with a repeated character and terminates it.
/// @param buffer the destination buffer.
/// @param c the character to repeat.
/// @param count how many times to repeat it.
static void __fill_name(char *buffer, char c, size_t count)
{
    memset(buffer, c, count);
    buffer[count] = 0;
}

/// @brief Creates a file and writes the given content into it.
/// @param path the file to create.
/// @param content the content to write.
/// @return 0 on success, -1 on failure.
static int __write_file(const char *path, const char *content)
{
    int fd = creat(path, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_path_bounds] creat(%zu-byte path): %s", strlen(path), strerror(errno));
        return -1;
    }
    size_t length   = strlen(content);
    ssize_t written = write(fd, content, length);
    close(fd);
    if (written != (ssize_t)length) {
        syslog(LOG_ERR, "[t_path_bounds] write returned %zd, expected %zu", written, length);
        return -1;
    }
    return 0;
}

/// @brief Reads a file and compares its content with the expected string.
/// @param path the file to read.
/// @param expected the expected content.
/// @return 0 when the content matches, -1 otherwise.
static int __check_content(const char *path, const char *expected)
{
    char buffer[64] = {0};
    int fd          = open(path, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_path_bounds] open(%zu-byte path): %s", strlen(path), strerror(errno));
        return -1;
    }
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (bytes < 0) {
        syslog(LOG_ERR, "[t_path_bounds] read: %s", strerror(errno));
        return -1;
    }
    buffer[bytes] = 0;
    if (strcmp(buffer, expected) != 0) {
        syslog(LOG_ERR, "[t_path_bounds] content is `%s`, expected `%s`", buffer, expected);
        return -1;
    }
    return 0;
}

/// @brief A file whose path exceeds 255 bytes must be created and read back.
/// @param dir the directory holding the file, its path is already over 255
///        bytes so the file component starts past the old truncation point.
/// @param file the resulting file path, filled by this function.
/// @return 0 on success, -1 on failure.
static int check_long_path(const char *dir, char *file)
{
    strcpy(file, dir);
    strcat(file, "/deep.txt");
    if (__write_file(file, "DEEPFILE") < 0) {
        return -1;
    }
    if (__check_content(file, "DEEPFILE") < 0) {
        return -1;
    }
    return 0;
}

/// @brief A name below an existing over-255-byte path must not resolve to
///        that path: the trailing component used to be dropped, so the
///        request silently reached the prefix instead.
/// @param dir the existing directory, over 255 bytes long.
/// @return 0 on success, -1 on failure.
static int check_no_prefix_resolution(const char *dir)
{
    char path[PATH_MAX] = {0};
    strcpy(path, dir);
    strcat(path, "/nonexistent");
    int fd = open(path, O_RDONLY, 0);
    if (fd >= 0) {
        close(fd);
        syslog(LOG_ERR, "[t_path_bounds] open of a nonexistent name below a long path succeeded");
        return -1;
    }
    if (errno != ENOENT) {
        syslog(LOG_ERR, "[t_path_bounds] expected ENOENT below a long path, got %s", strerror(errno));
        return -1;
    }
    return 0;
}

/// @brief The longest usable component, NAME_MAX - 1 characters, must work.
/// @param file the resulting file path, filled by this function.
/// @return 0 on success, -1 on failure.
static int check_max_component(char *file)
{
    char name[NAME_MAX + 1] = {0};
    __fill_name(name, 'm', NAME_MAX - 1);
    strcpy(file, BASE_DIR "/");
    strcat(file, name);
    if (__write_file(file, "MAXNAME") < 0) {
        return -1;
    }
    if (__check_content(file, "MAXNAME") < 0) {
        return -1;
    }
    return 0;
}

/// @brief A component of NAME_MAX characters or longer must be rejected with
///        ENAMETOOLONG instead of being truncated to an existing name.
/// @param length the component length to test.
/// @return 0 on success, -1 on failure.
static int check_component_rejected(size_t length)
{
    char name[PATH_MAX] = {0};
    char path[PATH_MAX] = {0};
    __fill_name(name, 'x', length);
    strcpy(path, BASE_DIR "/");
    strcat(path, name);
    int fd = creat(path, 0644);
    if (fd >= 0) {
        close(fd);
        unlink(path);
        syslog(LOG_ERR, "[t_path_bounds] creat of a %zu-character component succeeded", length);
        return -1;
    }
    if (errno != ENAMETOOLONG) {
        syslog(
            LOG_ERR, "[t_path_bounds] %zu-character component: expected ENAMETOOLONG, got %s", length,
            strerror(errno));
        return -1;
    }
    return 0;
}

int main(void)
{
    char dir[PATH_MAX]       = {0};
    char deep_file[PATH_MAX] = {0};
    char max_file[PATH_MAX]  = {0};
    char name[NAME_MAX + 1]  = {0};
    int failures             = 0;

    // The directory path is 27 + LONG_DIR_LEN bytes, over the 255-byte mark,
    // while the component itself stays below NAME_MAX.
    if ((mkdir(BASE_DIR, 0755) < 0) && (errno != EEXIST)) {
        syslog(LOG_ERR, "[t_path_bounds] mkdir(%s): %s", BASE_DIR, strerror(errno));
        return EXIT_FAILURE;
    }
    __fill_name(name, 'a', LONG_DIR_LEN);
    strcpy(dir, BASE_DIR "/");
    strcat(dir, name);
    if (mkdir(dir, 0755) < 0) {
        syslog(LOG_ERR, "[t_path_bounds] mkdir(%zu-byte path): %s", strlen(dir), strerror(errno));
        return EXIT_FAILURE;
    }

    if (check_long_path(dir, deep_file) < 0) {
        ++failures;
    }
    if (check_no_prefix_resolution(dir) < 0) {
        ++failures;
    }
    if (check_max_component(max_file) < 0) {
        ++failures;
    }
    if (check_component_rejected(NAME_MAX) < 0) {
        ++failures;
    }
    if (check_component_rejected(NAME_MAX + 45) < 0) {
        ++failures;
    }

    // Best-effort cleanup, so a second run on the same image starts clean.
    if (deep_file[0]) {
        unlink(deep_file);
    }
    if (max_file[0]) {
        unlink(max_file);
    }
    rmdir(dir);
    rmdir(BASE_DIR);

    if (failures == 0) {
        syslog(LOG_INFO, "[t_path_bounds] all path boundary checks passed");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_path_bounds] %d FAILURES", failures);
    return EXIT_FAILURE;
}
