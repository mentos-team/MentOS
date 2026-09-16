/// @file t_symlink.c
/// @brief Regression test for #288 and #371: resolving a path through a
/// symbolic link must neither write past the path buffer nor leave it
/// unterminated, and reading a link target must respect how ext2 stores
/// it.
/// @details `__resolve_path` substituted a link target with a plain
/// `memcpy` in two places: the relative branch copied `linklen` bytes at
/// the last slash with no bound, so a target substituted near the end of
/// an almost-full buffer ran past `char buffer[PATH_MAX]` on the kernel
/// stack, and neither branch wrote the terminator, so the bytes of the
/// path the link replaced stayed behind the link content whenever the
/// target was shorter than the name it replaced (#288). `ext2_readlink`
/// measured the target with `strlen` over a field ext2 does not
/// terminate, and had no branch for a target held in a data block, so a
/// long link came back as raw block indices (#371). The checks below
/// cover the observable consequences:
///   - a link whose target is shorter than its own name resolves to the
///     target, not to the leftover of its old name;
///   - targets of sixty and sixty-three characters, which the image
///     tools store in a data block, read back exactly and resolve;
///   - a link near the end of an almost-PATH_MAX path is rejected with
///     ENAMETOOLONG instead of overflowing the buffer;
///   - resolution still works after such a rejection.
///
/// The deep fixture is planted inside the image by
/// scripts/make-symlink-fixture.cmake when the image is packed, with
/// debugfs: `/.t_symlink_deep` holds a chain of directories that brings
/// the path of the trailing link `ln` to exactly PATH_MAX - 1 characters,
/// too long to stage through the host filesystem.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <dirent.h>
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

/// A shipped link with a relative target, as a baseline for resolution.
#define SHIPPED_LINK "/home/user/tmp.md"

/// A committed link whose name is longer than its target: without the
/// terminator write, the tail of the old name survived the substitution.
#define SHORT_TARGET_LINK "/home/user/link_with_a_name_much_longer_than_its_target"

/// A committed link whose target is exactly sixty characters: e2fsprogs
/// moves a target to a block at sixty bytes, so this is the shortest
/// block-held link the image can contain (#371).
#define SIXTY_LINK "/home/user/t_symlink_sixty"

/// The file the sixty-character link points to; its absolute path is
/// exactly sixty characters long.
#define SIXTY_TARGET_FILE "/home/user/t_symlink_sixtyeeeeeeeeeeeeeeeeeeeeeeeeeeeeee.txt"

/// A committed link whose target is sixty-three characters, also held in
/// a data block (#371).
#define SLOW_LINK "/home/user/t_symlink_slow"

/// The file the slow link points to, through a sixty-three character
/// absolute target.
#define SLOW_TARGET_FILE "/home/user/t_symlink_slowwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww.txt"

/// The file the links above point to.
#define WELCOME_FILE "/home/user/welcome.md"

/// Root of the generated deep fixture.
#define DEEP_ROOT "/.t_symlink_deep"

/// Name of the trailing link of the fixture.
#define DEEP_LINK_NAME "ln"

/// Guards the walk against a malformed fixture.
#define DEEP_MAX_LEVELS 32

/// @brief Reads a whole small file into a buffer.
/// @param path the file to read.
/// @param buffer the destination buffer.
/// @param buflen the size of the destination buffer.
/// @return the number of bytes read, or -1 on failure.
static ssize_t __read_file(const char *path, char *buffer, size_t buflen)
{
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_symlink] open(%s): %s", path, strerror(errno));
        return -1;
    }
    ssize_t bytes = read(fd, buffer, buflen - 1);
    close(fd);
    if (bytes < 0) {
        syslog(LOG_ERR, "[t_symlink] read(%s): %s", path, strerror(errno));
        return -1;
    }
    buffer[bytes] = 0;
    return bytes;
}

/// @brief Checks that a link resolves to the same content as its target.
/// @param link the link to read through.
/// @param target the file the link is expected to point to.
/// @return 0 on success, -1 on failure.
static int __check_resolution(const char *link, const char *target)
{
    char through_link[128] = {0};
    char direct[128]       = {0};
    if (__read_file(link, through_link, sizeof(through_link)) < 0) {
        return -1;
    }
    if (__read_file(target, direct, sizeof(direct)) < 0) {
        return -1;
    }
    if (strcmp(through_link, direct) != 0) {
        syslog(LOG_ERR, "[t_symlink] `%s` resolved to `%s`, expected `%s`", link, through_link, direct);
        return -1;
    }
    return 0;
}

/// @brief Checks that readlink reports the stored target of a link.
/// @param link the link to inspect.
/// @param expected the expected target string.
/// @return 0 on success, -1 on failure.
static int __check_readlink(const char *link, const char *expected)
{
    char target[128] = {0};
    ssize_t length   = readlink(link, target, sizeof(target) - 1);
    if (length < 0) {
        syslog(LOG_ERR, "[t_symlink] readlink(%s): %s", link, strerror(errno));
        return -1;
    }
    target[length] = 0;
    if (strcmp(target, expected) != 0) {
        syslog(LOG_ERR, "[t_symlink] readlink(%s) returned `%s`, expected `%s`", link, target, expected);
        return -1;
    }
    return 0;
}

/// @brief Walks the generated fixture down to its trailing link.
/// @details Every directory of the chain holds a single entry, so the
/// walk is a straight descent: a directory entry continues the chain, the
/// first non-directory entry is the link, whose path must be exactly
/// PATH_MAX - 1 characters long.
/// @param path the buffer receiving the link path.
/// @return 0 on success, -1 on failure.
static int __find_deep_link(char *path)
{
    strcpy(path, DEEP_ROOT);
    static dirent_t entries[4];
    for (int level = 0; level < DEEP_MAX_LEVELS; ++level) {
        int fd = open(path, O_RDONLY | O_DIRECTORY, 0);
        if (fd < 0) {
            syslog(LOG_ERR, "[t_symlink] open(%s): %s", path, strerror(errno));
            return -1;
        }
        char name[NAME_MAX + 1] = {0};
        unsigned short type     = 0;
        ssize_t bytes;
        while ((bytes = getdents(fd, entries, sizeof(entries))) > 0) {
            for (size_t i = 0; i < (size_t)bytes / sizeof(dirent_t); ++i) {
                if ((strcmp(entries[i].d_name, ".") != 0) && (strcmp(entries[i].d_name, "..") != 0)) {
                    strncpy(name, entries[i].d_name, NAME_MAX);
                    type = entries[i].d_type;
                    break;
                }
            }
            if (name[0]) {
                break;
            }
        }
        close(fd);
        if (name[0] == 0) {
            syslog(LOG_ERR, "[t_symlink] `%s` lists no entry", path);
            return -1;
        }
        size_t length = strlen(path);
        if (length + 1 + strlen(name) >= PATH_MAX) {
            syslog(LOG_ERR, "[t_symlink] the fixture path does not fit the buffer");
            return -1;
        }
        path[length] = '/';
        strcpy(path + length + 1, name);
        if (type != DT_DIR) {
            if (strcmp(name, DEEP_LINK_NAME) != 0) {
                syslog(LOG_ERR, "[t_symlink] unexpected non-directory entry `%s` in the fixture", name);
                return -1;
            }
            if (strlen(path) != PATH_MAX - 1) {
                syslog(LOG_ERR, "[t_symlink] the fixture link path is %zu characters, expected %d", strlen(path), PATH_MAX - 1);
                return -1;
            }
            return 0;
        }
    }
    syslog(LOG_ERR, "[t_symlink] the fixture is deeper than %d levels", DEEP_MAX_LEVELS);
    return -1;
}

/// @brief A link whose substitution does not fit the buffer must be
/// rejected with ENAMETOOLONG, not overflow it.
/// @return 0 on success, -1 on failure.
static int __check_deep_link(void)
{
    char path[PATH_MAX];
    if (__find_deep_link(path) < 0) {
        return -1;
    }
    int fd = open(path, O_RDONLY, 0);
    if (fd >= 0) {
        close(fd);
        syslog(LOG_ERR, "[t_symlink] open of a %zu-character path through a link succeeded", strlen(path));
        return -1;
    }
    if (errno != ENAMETOOLONG) {
        syslog(LOG_ERR, "[t_symlink] expected ENAMETOOLONG, got %s", strerror(errno));
        return -1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;

    if (__check_readlink(SHIPPED_LINK, "../user/welcome.md") < 0) {
        ++failures;
    }
    if (__check_resolution(SHIPPED_LINK, WELCOME_FILE) < 0) {
        ++failures;
    }
    if (__check_resolution(SHORT_TARGET_LINK, WELCOME_FILE) < 0) {
        ++failures;
    }
    // A target of exactly sixty characters is the length at which the
    // image tools move it to a data block: the read has to come from
    // there, not from the inline field read as block pointers (#371).
    if (__check_readlink(SIXTY_LINK, SIXTY_TARGET_FILE) < 0) {
        ++failures;
    }
    if (__check_resolution(SIXTY_LINK, SIXTY_TARGET_FILE) < 0) {
        ++failures;
    }
    // A target of sixty-three characters is held in a data block too.
    if (__check_readlink(SLOW_LINK, SLOW_TARGET_FILE) < 0) {
        ++failures;
    }
    if (__check_resolution(SLOW_LINK, SLOW_TARGET_FILE) < 0) {
        ++failures;
    }
    if (__check_deep_link() < 0) {
        ++failures;
    }
    // Resolution must still work after a rejection.
    if (__check_resolution(SHIPPED_LINK, WELCOME_FILE) < 0) {
        ++failures;
    }

    if (failures == 0) {
        syslog(LOG_INFO, "[t_symlink] all symlink resolution checks passed");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_symlink] %d FAILURES", failures);
    return EXIT_FAILURE;
}
