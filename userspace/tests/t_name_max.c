/// @file t_name_max.c
/// @brief Regression test for #285: names at the 255-character boundary.
/// @details Every name field in the kernel is a NUL-terminated `char[255]`:
/// `vfs_file_t::name`, `dirent_t::d_name` and the `name` of the direntry the
/// ext2 search fills in are all `NAME_MAX` bytes, and `EXT2_NAME_LEN` is 255
/// as well. A name of exactly 255 characters therefore has no room for its
/// terminator anywhere in the kernel, while the on-disk `uint8_t name_len`
/// makes 255 a perfectly legal value to store.
///
/// The longest name the kernel can represent is `NAME_MAX - 1`, so that is
/// the contract this checks: 254 characters work end to end, and anything
/// longer is refused with ENAMETOOLONG rather than created as a name the
/// kernel cannot then handle.
///
/// A name that is accepted has to survive the whole round trip, not just the
/// creation: it is written to, read back, found by `stat`, listed by
/// `getdents` at its full length, and removed. Listing it is the check that
/// matters most, because the copy into `d_name` is the one place where a
/// name of the maximum length fills the field exactly.
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

/// The directory the names are created in.
#define DIR "/home/user"

/// The longest name the kernel can hold, terminator included.
#define LONGEST 254

/// Buffer for the paths under test.
static char path[512];

/// Buffer for reading directory entries.
static char entries[sizeof(dirent_t) * 32];

/// @brief Builds DIR/<len characters>.
/// @param len how many characters the name has.
static void __build_path(unsigned len)
{
    unsigned pos = 0;
    memcpy(path, DIR "/", sizeof(DIR));
    pos = sizeof(DIR);
    for (unsigned i = 0; i < len; ++i) {
        path[pos++] = 'n';
    }
    path[pos] = 0;
}

/// @brief Looks for the name in the listing of DIR.
/// @param name the name to look for.
/// @param found_len where the length of the entry that was found is stored.
/// @return 1 when the name is listed, 0 when it is not, -1 on failure.
static int __listed(const char *name, unsigned *found_len)
{
    int fd = open(DIR, O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_name_max] open of " DIR ": %s", strerror(errno));
        return -1;
    }
    *found_len = 0;
    ssize_t bytes;
    while ((bytes = getdents(fd, (dirent_t *)entries, sizeof(entries))) > 0) {
        for (ssize_t off = 0; off < bytes; off += sizeof(dirent_t)) {
            dirent_t *entry = (dirent_t *)(entries + off);
            if (strcmp(entry->d_name, name) == 0) {
                *found_len = (unsigned)strlen(entry->d_name);
                close(fd);
                return 1;
            }
        }
    }
    close(fd);
    if (bytes < 0) {
        syslog(LOG_ERR, "[t_name_max] getdents: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/// @brief Checks that a name of the given length works end to end.
/// @param len how many characters the name has.
/// @return 0 on success, -1 on failure.
static int __check_accepted(unsigned len)
{
    __build_path(len);
    const char *name = path + sizeof(DIR);

    errno  = 0;
    int fd = creat(path, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_name_max] creating a name of %u characters failed: %s", len, strerror(errno));
        return -1;
    }
    const char *content = "name";
    ssize_t written     = write(fd, content, strlen(content));
    close(fd);
    if (written != (ssize_t)strlen(content)) {
        syslog(LOG_ERR, "[t_name_max] writing to the %u-character name returned %zd", len, written);
        unlink(path);
        return -1;
    }

    int failures = 0;

    // The name has to be found again by the very lookup that stores it in a
    // buffer of exactly NAME_MAX bytes.
    stat_t st;
    if (stat(path, &st) < 0) {
        syslog(LOG_ERR, "[t_name_max] stat of the %u-character name: %s", len, strerror(errno));
        ++failures;
    } else if ((unsigned)st.st_size != strlen(content)) {
        syslog(LOG_ERR, "[t_name_max] the %u-character name is %u bytes, expected %u", len, (unsigned)st.st_size, (unsigned)strlen(content));
        ++failures;
    }

    // And it has to be listed at its full length: a copy that fills d_name
    // without terminating it reads as a longer name, or as a different one.
    unsigned found_len = 0;
    int listed         = __listed(name, &found_len);
    if (listed < 0) {
        ++failures;
    } else if (listed == 0) {
        syslog(LOG_ERR, "[t_name_max] the %u-character name is not in the listing of " DIR, len);
        ++failures;
    } else if (found_len != len) {
        syslog(LOG_ERR, "[t_name_max] the %u-character name is listed with %u characters", len, found_len);
        ++failures;
    }

    // Reading it back proves the direntry points where it should.
    char buffer[16] = {0};
    fd              = open(path, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_name_max] reopening the %u-character name: %s", len, strerror(errno));
        ++failures;
    } else {
        ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);
        if ((bytes != (ssize_t)strlen(content)) || (strcmp(buffer, content) != 0)) {
            syslog(LOG_ERR, "[t_name_max] the %u-character name reads back `%s`, expected `%s`", len, buffer, content);
            ++failures;
        }
    }

    if (unlink(path) < 0) {
        syslog(LOG_ERR, "[t_name_max] unlink of the %u-character name: %s", len, strerror(errno));
        ++failures;
    }
    return (failures == 0) ? 0 : -1;
}

/// @brief Checks that a name of the given length is refused.
/// @param len how many characters the name has.
/// @return 0 on success, -1 on failure.
static int __check_refused(unsigned len)
{
    __build_path(len);

    errno  = 0;
    int fd = creat(path, 0644);
    if (fd >= 0) {
        // A name the kernel cannot terminate must not reach the disk: every
        // later lookup of it copies name_len bytes into a NAME_MAX buffer.
        close(fd);
        syslog(LOG_ERR, "[t_name_max] creating a name of %u characters succeeded, it had to be refused", len);
        unlink(path);
        return -1;
    }
    if (errno != ENAMETOOLONG) {
        int reported = errno;
        syslog(LOG_ERR, "[t_name_max] a name of %u characters reported errno %d: %s", len, reported, strerror(reported));
        syslog(LOG_ERR, "[t_name_max] the expected errno was %d: %s", ENAMETOOLONG, strerror(ENAMETOOLONG));
        return -1;
    }
    return 0;
}

/// @brief Checks that mkdir applies the same bound as creat.
/// @param len how many characters the name has.
/// @param accepted whether the length has to be accepted.
/// @return 0 on success, -1 on failure.
static int __check_mkdir(unsigned len, int accepted)
{
    __build_path(len);

    errno   = 0;
    int ret = mkdir(path, 0755);
    if (accepted) {
        if (ret < 0) {
            syslog(LOG_ERR, "[t_name_max] mkdir of a %u-character name failed: %s", len, strerror(errno));
            return -1;
        }
        if (rmdir(path) < 0) {
            syslog(LOG_ERR, "[t_name_max] rmdir of the %u-character name: %s", len, strerror(errno));
            return -1;
        }
        return 0;
    }
    if (ret >= 0) {
        syslog(LOG_ERR, "[t_name_max] mkdir of a %u-character name succeeded, it had to be refused", len);
        rmdir(path);
        return -1;
    }
    if (errno != ENAMETOOLONG) {
        int reported = errno;
        syslog(LOG_ERR, "[t_name_max] mkdir of a %u-character name reported errno %d: %s", len, reported, strerror(reported));
        syslog(LOG_ERR, "[t_name_max] the expected errno was %d: %s", ENAMETOOLONG, strerror(ENAMETOOLONG));
        return -1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;

    // Well inside the bound, so a failure here is not about the boundary.
    if (__check_accepted(LONGEST - 1) < 0) {
        ++failures;
    }
    // Exactly the longest name the kernel can hold.
    if (__check_accepted(LONGEST) < 0) {
        ++failures;
    }
    // One past it: 255 fits the on-disk name_len but leaves no room for a
    // terminator in any of the kernel's NAME_MAX buffers.
    if (__check_refused(LONGEST + 1) < 0) {
        ++failures;
    }
    // And beyond, where the on-disk name_len itself would wrap.
    if (__check_refused(LONGEST + 2) < 0) {
        ++failures;
    }

    // Directories carry the same names through the same copies.
    if (__check_mkdir(LONGEST, 1) < 0) {
        ++failures;
    }
    if (__check_mkdir(LONGEST + 1, 0) < 0) {
        ++failures;
    }

    if (failures == 0) {
        syslog(LOG_INFO, "[t_name_max] names up to %u characters work, longer ones are refused", (unsigned)LONGEST);
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_name_max] %d FAILURES", failures);
    return EXIT_FAILURE;
}
