/// @file t_namei_io.c
/// @brief Regression test for #353: an I/O error while resolving a path must
/// not be read as "this component is not a symbolic link".
/// @details `__resolve_path` asks `__is_a_link` about every component, and
/// `__is_a_link` answered 0 whenever `vfs_stat` failed. `vfs_stat` fails for
/// two entirely different reasons — the component is not there, or it could
/// not be read — and both came back as "not a link". A component that *is* a
/// link but whose inode could not be read was therefore resolved as an
/// ordinary name, and the operation went on along the path the link would have
/// redirected: it lands on a different file from the one the caller named,
/// which is the class of outcome #284 is about.
///
/// The reason now travels: `ext2_find_direntry` reports -EIO for a failed
/// inode read instead of the -1 that meant everything, `ext2_resolve_path` and
/// `ext2_stat` pass it on instead of flattening it to -ENOENT, and
/// `__is_a_link` takes only -ENOENT as "not a link" and hands anything else to
/// the caller.
///
/// The first read of any path resolution is the inode of the root directory,
/// so a plain count of failures is enough to reach it; the driver retries a
/// sector three times, so three failures are armed. Opening a file that
/// exists then has to fail with EIO rather than succeed.
///
/// Without ENABLE_ATA_FAULT_INJECTION there is no way to make the read fail,
/// so the test reports the facility as absent and passes.
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

/// The control file of the fault injection.
#define CONTROL "/proc/faultinj"

/// The file the test opens. It exists, so nothing but the injected failure can
/// keep the open from succeeding.
#define TARGET "/home/user/t_namei_io.txt"

/// How many times the driver retries a failed sector, and so how many failures
/// have to be armed for one read to actually fail.
#define ATTEMPTS 3

/// @brief Reads a counter out of the control file.
/// @param name the counter to read.
/// @param value where the value is stored.
/// @return 0 on success, -1 on failure.
static int __counter(const char *name, unsigned *value)
{
    int fd = open(CONTROL, O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }
    char buffer[256] = {0};
    ssize_t bytes    = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (bytes <= 0) {
        return -1;
    }
    char *at = strstr(buffer, name);
    if (at == NULL) {
        syslog(LOG_ERR, "[t_namei_io] " CONTROL " does not report `%s`", name);
        return -1;
    }
    at += strlen(name);
    while (*at == ' ') {
        ++at;
    }
    *value = 0;
    for (; (*at >= '0') && (*at <= '9'); ++at) {
        *value = (*value * 10U) + (unsigned)(*at - '0');
    }
    return 0;
}

/// @brief Sends a command to the fault injection.
/// @param command what to send.
/// @return 0 on success, -1 on failure.
static int __arm(const char *command)
{
    int fd = open(CONTROL, O_WRONLY, 0);
    if (fd < 0) {
        return -1;
    }
    ssize_t written = write(fd, command, strlen(command));
    close(fd);
    return (written == (ssize_t)strlen(command)) ? 0 : -1;
}

int main(void)
{
    int probe = open(CONTROL, O_RDONLY, 0);
    if (probe < 0) {
        syslog(LOG_INFO, "[t_namei_io] " CONTROL " is absent: built without ENABLE_ATA_FAULT_INJECTION, nothing to do");
        return EXIT_SUCCESS;
    }
    close(probe);

    // Create the file the resolution will be aimed at.
    int fd = open(TARGET, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_namei_io] creating %s: %s", TARGET, strerror(errno));
        return EXIT_FAILURE;
    }
    if (write(fd, "namei", 5) != 5) {
        syslog(LOG_ERR, "[t_namei_io] writing %s: %s", TARGET, strerror(errno));
        close(fd);
        unlink(TARGET);
        return EXIT_FAILURE;
    }
    close(fd);

    int failures = 0;

    // Fail the first read of the resolution, which is the inode of the root
    // directory: every component of the path is below it.
    char command[32];
    sprintf(command, "read %d", ATTEMPTS);
    if (__arm(command) < 0) {
        syslog(LOG_ERR, "[t_namei_io] arming `%s`: %s", command, strerror(errno));
        unlink(TARGET);
        return EXIT_FAILURE;
    }

    errno          = 0;
    int broken_fd  = open(TARGET, O_RDONLY, 0);
    int open_errno = errno;
    if (broken_fd >= 0) {
        close(broken_fd);
    }

    unsigned injected = 0;
    if (__counter("reads_injected", &injected) < 0) {
        ++failures;
    } else if (injected != ATTEMPTS) {
        syslog(
            LOG_ERR, "[t_namei_io] the %d armed failures fired %u times: nothing was tested", ATTEMPTS, injected);
        ++failures;
    } else if (broken_fd >= 0) {
        syslog(
            LOG_ERR,
            "[t_namei_io] opening %s succeeded while the root directory could not be read: the error was read as "
            "`not a symbolic link` and the resolution carried on",
            TARGET);
        ++failures;
    } else if (open_errno != EIO) {
        syslog(
            LOG_ERR, "[t_namei_io] opening %s failed with `%s`, expected an I/O error", TARGET, strerror(open_errno));
        ++failures;
    }

    if (__arm("off") < 0) {
        syslog(LOG_ERR, "[t_namei_io] disarming: %s", strerror(errno));
        ++failures;
    }

    // The failure has to be transient: once nothing is armed the same open
    // works again, so what failed above was the injected read and not the
    // path itself.
    fd = open(TARGET, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_namei_io] %s does not open after disarming: %s", TARGET, strerror(errno));
        ++failures;
    } else {
        close(fd);
    }
    unlink(TARGET);

    if (failures == 0) {
        syslog(LOG_INFO, "[t_namei_io] an unreadable component stops the resolution instead of being walked past");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_namei_io] %d FAILURES", failures);
    return EXIT_FAILURE;
}
