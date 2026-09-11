/// @file t_mount_boundary.c
/// @brief Regression test for #289 item 4: a mount point must only claim the
/// paths inside it.
/// @details `vfs_get_superblock` picks the mount whose path is the longest
/// prefix of the path being looked up, and compared only those characters:
///
///     len = strlen(sb->path);
///     if (!strncmp(path, sb->path, len)) { ... }
///
/// Nothing checked that the prefix ended where a path component ends, so a
/// mount at `/dev/null` also claimed `/dev/nullx`, and one at `/proc` claimed
/// `/procx`. The issue recorded this as harmless with today's mounts. It is
/// not: there are four of them — `/`, `/proc`, `/dev/null` and `/dev/hda` —
/// and `open("/dev/nullx", O_RDONLY)` used to *succeed*, so any name starting
/// with those nine characters was the null device.
///
/// Every check comes with the control that the mount it borders on still
/// works, because the cheapest way to pass this test would be to stop
/// matching mount points at all.
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

/// A path that extends the `/dev/null` mount point without being inside it.
#define BEYOND_NULL "/dev/nullx"

/// A path that extends the `/proc` mount point without being inside it.
#define BEYOND_PROC "/procx"

/// @brief Checks that a path bordering a mount point is not claimed by it.
/// @return 0 on success, -1 on failure.
static int __check_beyond_null(void)
{
    errno  = 0;
    int fd = open(BEYOND_NULL, O_RDONLY, 0);
    if (fd >= 0) {
        close(fd);
        syslog(LOG_ERR, "[t_mount_boundary] opening " BEYOND_NULL " succeeded: it is the null device");
        return -1;
    }
    if (errno != ENOENT) {
        int reported = errno;
        syslog(LOG_ERR, "[t_mount_boundary] opening " BEYOND_NULL " reported errno %d: %s", reported, strerror(reported));
        syslog(LOG_ERR, "[t_mount_boundary] the expected errno was %d: %s", ENOENT, strerror(ENOENT));
        return -1;
    }
    return 0;
}

/// @brief Control: the null device itself still works.
/// @return 0 on success, -1 on failure.
static int __check_null_device(void)
{
    int fd = open("/dev/null", O_WRONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_mount_boundary] opening /dev/null failed: %s", strerror(errno));
        return -1;
    }
    ssize_t written = write(fd, "discarded", 9);
    close(fd);
    if (written != 9) {
        syslog(LOG_ERR, "[t_mount_boundary] writing to /dev/null returned %zd, expected 9", written);
        return -1;
    }
    return 0;
}

/// @brief Checks that a path bordering /proc lands on the root filesystem.
/// @details Being routed to procfs did not merely give the wrong error, it
///          gave a *different* one for every operation: creating the file
///          reported ENOSYS because procfs has no creat, while `stat` on it
///          reported EPERM. On the root filesystem it is an ordinary file.
/// @return 0 on success, -1 on failure.
static int __check_beyond_proc(void)
{
    errno  = 0;
    int fd = creat(BEYOND_PROC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_mount_boundary] creating " BEYOND_PROC " failed: %s", strerror(errno));
        return -1;
    }
    const char *content = "root";
    ssize_t written     = write(fd, content, strlen(content));
    close(fd);
    if (written != (ssize_t)strlen(content)) {
        syslog(LOG_ERR, "[t_mount_boundary] writing to " BEYOND_PROC " returned %zd", written);
        unlink(BEYOND_PROC);
        return -1;
    }
    int failures = 0;
    stat_t st;
    if (stat(BEYOND_PROC, &st) < 0) {
        syslog(LOG_ERR, "[t_mount_boundary] stat of " BEYOND_PROC ": %s", strerror(errno));
        ++failures;
    } else if ((unsigned)st.st_size != strlen(content)) {
        syslog(LOG_ERR, "[t_mount_boundary] " BEYOND_PROC " is %u bytes, expected %u", (unsigned)st.st_size, (unsigned)strlen(content));
        ++failures;
    }
    if (unlink(BEYOND_PROC) < 0) {
        syslog(LOG_ERR, "[t_mount_boundary] unlink of " BEYOND_PROC ": %s", strerror(errno));
        ++failures;
    }
    return (failures == 0) ? 0 : -1;
}

/// @brief Control: /proc itself still works.
/// @return 0 on success, -1 on failure.
static int __check_procfs(void)
{
    int fd = open("/proc/mounts", O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_mount_boundary] opening /proc/mounts failed: %s", strerror(errno));
        return -1;
    }
    char buffer[256] = {0};
    ssize_t bytes    = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (bytes <= 0) {
        syslog(LOG_ERR, "[t_mount_boundary] reading /proc/mounts returned %zd", bytes);
        return -1;
    }
    // The root mount has to be in there, otherwise the read said nothing.
    if (strstr(buffer, " / ") == NULL) {
        syslog(LOG_ERR, "[t_mount_boundary] /proc/mounts does not list the root mount");
        return -1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;

    if (__check_beyond_null() < 0) {
        ++failures;
    }
    if (__check_null_device() < 0) {
        ++failures;
    }
    if (__check_beyond_proc() < 0) {
        ++failures;
    }
    if (__check_procfs() < 0) {
        ++failures;
    }

    if (failures == 0) {
        syslog(LOG_INFO, "[t_mount_boundary] mount points claim only the paths inside them");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_mount_boundary] %d FAILURES", failures);
    return EXIT_FAILURE;
}
