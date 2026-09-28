/// @file t_sync.c
/// @brief Regression test for #408: the sync family must be reachable.
/// @details `sys_sync`, `sys_syncfs` and `sys_sync_file_range` were
/// implemented, declared, and assigned system-call numbers, and then never
/// registered in `sys_call_table`. There was no wrapper and no prototype
/// either, so userspace could not reach them at all and the implementations
/// had never run.
///
/// They do no work, and that is correct rather than unfinished: nothing in
/// this kernel buffers a write, so by the time a write returns there is
/// nothing left to flush. What they do owe the caller is the error contract,
/// which is what this test pins — succeeding for a descriptor nobody opened
/// would hide the caller's own bug.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <strerror.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

/// A file the test creates, so that it has a descriptor that is really open.
#define SYNC_PATH "/home/user/t_sync.tmp"

/// A descriptor number no task has open.
#define CLOSED_FD 4096

/// @brief Expects a call to fail with a specific errno.
/// @param what the description of the call, for the failure message.
/// @param expected the errno the call must report.
/// @param expression the call, evaluating to its return value.
#define EXPECT_ERRNO(what, expected, expression)                                                            \
    do {                                                                                                    \
        errno = 0;                                                                                          \
        if ((expression) != -1) {                                                                           \
            syslog(LOG_ERR, "[t_sync] %s succeeded, expected failure", what);                               \
            return EXIT_FAILURE;                                                                            \
        }                                                                                                   \
        if (errno != (expected)) {                                                                          \
            syslog(LOG_ERR, "[t_sync] %s: expected %s, got %s", what, strerror(expected), strerror(errno)); \
            return EXIT_FAILURE;                                                                            \
        }                                                                                                   \
    } while (0)

int main(void)
{
    openlog("t_sync", LOG_CONS | LOG_PID, LOG_USER);

    // Reaching the call at all is half of what this test is for: before the
    // fix the dispatcher had no entry and this returned -ENOSYS.
    if (sync() != 0) {
        syslog(LOG_ERR, "[t_sync] sync(): %s", strerror(errno));
        return EXIT_FAILURE;
    }

    int fd = creat(SYNC_PATH, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_sync] creat(%s): %s", SYNC_PATH, strerror(errno));
        return EXIT_FAILURE;
    }
    if (write(fd, "sync", 4) != 4) {
        syslog(LOG_ERR, "[t_sync] write: %s", strerror(errno));
        close(fd);
        unlink(SYNC_PATH);
        return EXIT_FAILURE;
    }

    // The happy paths, on a descriptor that is genuinely open.
    if (syncfs(fd) != 0) {
        syslog(LOG_ERR, "[t_sync] syncfs on an open descriptor: %s", strerror(errno));
        close(fd);
        unlink(SYNC_PATH);
        return EXIT_FAILURE;
    }
    if (sync_file_range(fd, 0, 0, SYNC_FILE_RANGE_WRITE) != 0) {
        syslog(LOG_ERR, "[t_sync] sync_file_range over the whole file: %s", strerror(errno));
        close(fd);
        unlink(SYNC_PATH);
        return EXIT_FAILURE;
    }

    // A negative range is nonsense whatever the descriptor is.
    errno = 0;
    if ((sync_file_range(fd, -1, 0, 0) != -1) || (errno != EINVAL)) {
        syslog(LOG_ERR, "[t_sync] sync_file_range with a negative offset: expected EINVAL, got %s", strerror(errno));
        close(fd);
        unlink(SYNC_PATH);
        return EXIT_FAILURE;
    }

    close(fd);
    unlink(SYNC_PATH);

    // A descriptor nobody holds is an error, not a silent success. A
    // negative one and one merely out of range must answer the same way.
    EXPECT_ERRNO("syncfs of a negative descriptor", EBADF, syncfs(-1));
    EXPECT_ERRNO("syncfs of a descriptor out of range", EBADF, syncfs(CLOSED_FD));
    EXPECT_ERRNO("syncfs of a descriptor that was closed", EBADF, syncfs(fd));
    EXPECT_ERRNO("sync_file_range of a negative descriptor", EBADF, sync_file_range(-1, 0, 0, 0));
    EXPECT_ERRNO("sync_file_range of a descriptor that was closed", EBADF, sync_file_range(fd, 0, 0, 0));

    syslog(LOG_INFO, "[t_sync] OK");
    return EXIT_SUCCESS;
}
