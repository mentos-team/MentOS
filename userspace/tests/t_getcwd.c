/// @file t_getcwd.c
/// @brief Test program for the getcwd system call buffer contract.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define SENTINEL 0xAA

static int check_success(const char *what, char *buf, size_t size, char *ret, const char *expected)
{
    if ((ret == (char *)-1) || (ret == NULL)) {
        syslog(LOG_ERR, "[t_getcwd] %s: expected success, got errno %d (%s)\n", what, errno, strerror(errno));
        return -1;
    }
    if (memchr(buf, '\0', size) == NULL) {
        syslog(LOG_ERR, "[t_getcwd] %s: result is not NUL-terminated within %u bytes\n", what, (unsigned)size);
        return -1;
    }
    if (strcmp(buf, expected) != 0) {
        syslog(LOG_ERR, "[t_getcwd] %s: expected `%s`, got `%s`\n", what, expected, buf);
        return -1;
    }
    return 0;
}

static int check_failure(const char *what, char *ret, int expected_errno)
{
    // POSIX: NULL, and only NULL. This used to accept `(char *)-1` as well,
    // because the wrapper could not return NULL and the test had to take what
    // it was given (#231).
    if (ret != NULL) {
        if (ret == (char *)-1) {
            syslog(LOG_ERR, "[t_getcwd] %s: failure reported as (char *)-1 instead of NULL\n", what);
        } else {
            syslog(LOG_ERR, "[t_getcwd] %s: expected failure, got success pointer\n", what);
        }
        return -1;
    }
    if (errno != expected_errno) {
        syslog(LOG_ERR, "[t_getcwd] %s: expected errno %d (%s), got %d\n", what, expected_errno, strerror(expected_errno), errno);
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    openlog("t_getcwd", LOG_PID | LOG_CONS, LOG_USER);

    const char *directory = "/home";
    if (chdir(directory) < 0) {
        syslog(LOG_ERR, "[t_getcwd] chdir failed: errno %d (%s)\n", errno, strerror(errno));
        closelog();
        return EXIT_FAILURE;
    }

    char buf[1024];
    char *ret;
    int failed = 0;

    // 1. A sufficiently large buffer must succeed and be NUL-terminated.
    memset(buf, SENTINEL, sizeof(buf));
    errno = 0;
    ret   = getcwd(buf, sizeof(buf));
    if (check_success("large buffer", buf, sizeof(buf), ret, directory) < 0) {
        failed = 1;
    }
    size_t len = strlen(buf);

    // 2. A buffer of exactly len + 1 bytes (terminator included) must succeed.
    memset(buf, SENTINEL, sizeof(buf));
    errno = 0;
    ret   = getcwd(buf, len + 1);
    if (check_success("exact-size buffer", buf, len + 1, ret, directory) < 0) {
        failed = 1;
    }

    // 3. A buffer one byte too small must fail with ERANGE.
    memset(buf, SENTINEL, sizeof(buf));
    errno = 0;
    ret   = getcwd(buf, len);
    if (check_failure("one-byte-too-small buffer", ret, ERANGE) < 0) {
        failed = 1;
    }

    // 3b. A clearly undersized buffer must fail with ERANGE as well.
    memset(buf, SENTINEL, sizeof(buf));
    errno = 0;
    ret   = getcwd(buf, 2);
    if (check_failure("undersized buffer", ret, ERANGE) < 0) {
        failed = 1;
    }

    // 4. size == 0 must fail with EINVAL (POSIX.1-2017).
    memset(buf, SENTINEL, sizeof(buf));
    errno = 0;
    ret   = getcwd(buf, 0);
    if (check_failure("zero size", ret, EINVAL) < 0) {
        failed = 1;
    }

    closelog();
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
