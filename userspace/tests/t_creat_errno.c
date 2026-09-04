/// @file t_creat_errno.c
/// @brief Regression test for #305: a creation that fails must say why.
/// @details Every path out of ext2_creat returned NULL without touching
/// errno, and vfs_creat then overwrote whatever was there with ENOENT, so a
/// full filesystem, a read-only device, a name that is too long and a request
/// to create the root directory were all reported to user space as "no such
/// file or directory". vfs_creat even printed the right reason on the line
/// above and threw it away on the next one (#289, item 1).
///
/// The two halves have to be fixed together to be observable at all: giving
/// ext2_creat correct errnos changes nothing while vfs_creat replaces them,
/// and keeping vfs_creat's value changes nothing while ext2_creat leaves it
/// unset.
///
/// The failures checked here are the ones that need no particular state of
/// the filesystem, so they are cheap and deterministic. The rollback of the
/// inode itself, which needs a filesystem with no room left, is exercised by
/// t_nospace.
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

/// A file that the test creates and removes, to check the happy path.
#define GOOD_PATH "/home/user/t_creat_errno.tmp"

/// A path whose parent directory does not exist.
#define NO_PARENT_PATH "/home/user/t_creat_errno.d/file"

/// @brief Checks that creating the given path fails with the given errno.
/// @param path the path to create.
/// @param expected the errno the attempt has to report.
/// @param what a description of the case, for the log.
/// @return 0 when the attempt failed as expected, -1 otherwise.
static int check_failure(const char *path, int expected, const char *what)
{
    errno  = 0;
    int fd = creat(path, 0644);
    if (fd >= 0) {
        close(fd);
        unlink(path);
        syslog(LOG_ERR, "[t_creat_errno] creating %s (%s) succeeded, it had to fail", path, what);
        return -1;
    }
    if (errno != expected) {
        // One strerror per line: it returns a single static buffer, so two
        // calls in the same message both show the second string and the log
        // reads as though the test compared a value with itself.
        int reported = errno;
        syslog(LOG_ERR, "[t_creat_errno] creating %s (%s) reported errno %d: %s", path, what, reported, strerror(reported));
        syslog(LOG_ERR, "[t_creat_errno] the expected errno was %d: %s", expected, strerror(expected));
        return -1;
    }
    return 0;
}

/// @brief Checks that an ordinary creation still works.
/// @return 0 on success, -1 on failure.
static int check_success(void)
{
    int fd = creat(GOOD_PATH, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_creat_errno] creating %s failed: %s", GOOD_PATH, strerror(errno));
        return -1;
    }
    // The file has to be usable, not merely created: the rollback added to
    // ext2_creat must not release anything on the way out of a success.
    const char *content = "creat";
    ssize_t written     = write(fd, content, strlen(content));
    close(fd);
    if (written != (ssize_t)strlen(content)) {
        syslog(LOG_ERR, "[t_creat_errno] write returned %zd, expected %u", written, (unsigned)strlen(content));
        unlink(GOOD_PATH);
        return -1;
    }
    char buffer[16] = {0};
    fd              = open(GOOD_PATH, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_creat_errno] reopening %s failed: %s", GOOD_PATH, strerror(errno));
        unlink(GOOD_PATH);
        return -1;
    }
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    unlink(GOOD_PATH);
    if ((bytes != (ssize_t)strlen(content)) || (strcmp(buffer, content) != 0)) {
        syslog(LOG_ERR, "[t_creat_errno] %s reads back `%s`, expected `%s`", GOOD_PATH, buffer, content);
        return -1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;

    // The root is its own parent, so creating it is a request to create a
    // directory. This is the case that used to come back as ENOENT.
    if (check_failure("/", EISDIR, "the root directory") < 0) {
        ++failures;
    }

    // A missing parent directory is genuinely ENOENT, and has to stay so:
    // reporting the underlying error must not turn the common case into
    // something else.
    if (check_failure(NO_PARENT_PATH, ENOENT, "a missing parent directory") < 0) {
        ++failures;
    }

    if (check_success() < 0) {
        ++failures;
    }

    if (failures == 0) {
        syslog(LOG_INFO, "[t_creat_errno] every failed creation reported its own reason");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_creat_errno] %d FAILURES", failures);
    return EXIT_FAILURE;
}
