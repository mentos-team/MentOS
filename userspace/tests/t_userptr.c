/// @file t_userptr.c
/// @brief Regression test for #191, stage one: the syscalls that move the
/// most bytes on behalf of a caller must refuse a pointer that does not
/// name the caller's memory.
/// @details There is no user-pointer validation layer: syscall handlers
/// dereference raw caller pointers with supervisor rights, so `read`
/// wrote wherever its buffer pointed — including kernel memory — and a
/// pointer to unmapped memory took the whole kernel down with a page
/// fault inside the handler. This test pins the first four gated
/// syscalls (`read`, `write`, `time`, `pipe`): each must answer `-EFAULT`
/// for a kernel pointer, an address straddling the kernel boundary, the
/// identity-mapped supervisor-only first megabyte every address space
/// inherits, an unmapped user address, and NULL, while the legitimate
/// uses keep working and the kernel survives it all.
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
#include <time.h>
#include <unistd.h>

/// Start of the kernel area: above this, nothing belongs to a task.
#define KERNEL_TOP ((void *)0xC0000000UL)

/// The video buffer inside the identity-mapped first megabyte: present in
/// every page directory, but supervisor-only, so the hardware never
/// protects the kernel from a syscall reaching for it.
#define VGA_MEMORY ((void *)0x000B8000UL)

/// An address nothing maps: above the first megabyte, below the lowest
/// mapped user segment.
#define UNMAPPED_USER ((void *)0x00200000UL)

/// @brief Expects a call to fail with EFAULT.
/// @param what the description of the call, for the failure message.
/// @param expression the call, evaluating to its return value.
/// @return 0 when the call failed with EFAULT, -1 otherwise.
#define EXPECT_EFAULT(what, expression)                                                        \
    do {                                                                                       \
        errno = 0;                                                                             \
        if ((expression) != -1) {                                                              \
            syslog(LOG_ERR, "[t_userptr] %s succeeded, expected EFAULT", what);                \
            return -1;                                                                         \
        }                                                                                      \
        if (errno != EFAULT) {                                                                 \
            syslog(LOG_ERR, "[t_userptr] %s: expected EFAULT, got %s", what, strerror(errno)); \
            return -1;                                                                         \
        }                                                                                      \
    } while (0)

/// @brief A bad read buffer must never reach the filesystem layer.
/// @param fd the descriptor to read from.
/// @return 0 on success, -1 on failure.
static int check_read_pointers(int fd)
{
    char buffer[8] = {0};
    EXPECT_EFAULT("read into the kernel area", read(fd, KERNEL_TOP, 4));
    EXPECT_EFAULT("read across the kernel boundary", read(fd, (char *)KERNEL_TOP - 2, 4));
    EXPECT_EFAULT("read into the supervisor-only first megabyte", read(fd, VGA_MEMORY, 4));
    EXPECT_EFAULT("read into unmapped memory", read(fd, UNMAPPED_USER, 4));
    EXPECT_EFAULT("read into NULL", read(fd, NULL, 4));
    // The legitimate use must keep working.
    if (read(fd, buffer, sizeof(buffer)) < 0) {
        syslog(LOG_ERR, "[t_userptr] read into a real buffer: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/// @brief A bad write buffer must never reach the filesystem layer.
/// @param fd the descriptor to write to.
/// @return 0 on success, -1 on failure.
static int check_write_pointers(int fd)
{
    const char text[] = "USERPTR";
    EXPECT_EFAULT("write from the kernel area", write(fd, KERNEL_TOP, 4));
    EXPECT_EFAULT("write from the supervisor-only first megabyte", write(fd, VGA_MEMORY, 4));
    EXPECT_EFAULT("write from unmapped memory", write(fd, UNMAPPED_USER, 4));
    // The legitimate use must keep working, and be readable back.
    if (write(fd, text, sizeof(text)) != (ssize_t)sizeof(text)) {
        syslog(LOG_ERR, "[t_userptr] write from a real buffer: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/// @brief The scalar outputs of time and the descriptor pair of pipe are
///        stored through caller pointers too.
/// @return 0 on success, -1 on failure.
static int check_scalar_outputs(void)
{
    time_t now = 0;
    EXPECT_EFAULT("time into the kernel area", time((time_t *)KERNEL_TOP));
    // time(NULL) is a legitimate request for the value alone.
    if (time(NULL) == (time_t)-1) {
        syslog(LOG_ERR, "[t_userptr] time(NULL): %s", strerror(errno));
        return -1;
    }
    if (time(&now) == (time_t)-1) {
        syslog(LOG_ERR, "[t_userptr] time into a real pointer: %s", strerror(errno));
        return -1;
    }
    int fds[2];
    EXPECT_EFAULT("pipe into the kernel area", pipe((int *)KERNEL_TOP));
    EXPECT_EFAULT("pipe into unmapped memory", pipe((int *)UNMAPPED_USER));
    EXPECT_EFAULT("pipe into NULL", pipe(NULL));
    if (pipe(fds) < 0) {
        syslog(LOG_ERR, "[t_userptr] pipe into a real array: %s", strerror(errno));
        return -1;
    }
    close(fds[0]);
    close(fds[1]);
    return 0;
}

int main(void)
{
    int failures = 0;

    // The file every test can read.
    int rfd = open("/home/user/welcome.md", O_RDONLY, 0);
    if (rfd < 0) {
        syslog(LOG_ERR, "[t_userptr] open(welcome.md): %s", strerror(errno));
        return EXIT_FAILURE;
    }
    // The scratch file nobody minds rewriting.
    int wfd = creat("/home/user/t_userptr.d/scratch.txt", 0644);
    if (wfd < 0) {
        if ((mkdir("/home/user/t_userptr.d", 0755) < 0) && (errno != EEXIST)) {
            syslog(LOG_ERR, "[t_userptr] mkdir: %s", strerror(errno));
            close(rfd);
            return EXIT_FAILURE;
        }
        wfd = creat("/home/user/t_userptr.d/scratch.txt", 0644);
        if (wfd < 0) {
            syslog(LOG_ERR, "[t_userptr] creat: %s", strerror(errno));
            close(rfd);
            return EXIT_FAILURE;
        }
    }

    if (check_read_pointers(rfd) < 0) {
        ++failures;
    }
    if (check_write_pointers(wfd) < 0) {
        ++failures;
    }
    if (check_scalar_outputs() < 0) {
        ++failures;
    }
    close(rfd);
    close(wfd);

    // Best-effort cleanup, so a second run on the same image starts clean.
    unlink("/home/user/t_userptr.d/scratch.txt");
    rmdir("/home/user/t_userptr.d");

    if (failures == 0) {
        syslog(LOG_INFO, "[t_userptr] all user-pointer checks passed");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_userptr] %d FAILURES", failures);
    return EXIT_FAILURE;
}
