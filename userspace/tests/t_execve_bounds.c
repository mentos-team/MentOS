/// @file t_execve_bounds.c
/// @brief Regression test for #190: sys_execve must copy the user-provided
/// argv[0] and filename into kernel buffers without overflowing them, must
/// keep the resulting task name NUL-terminated, and must reject filenames
/// that do not fit PATH_MAX instead of truncating them.
/// @details The original defect: `strcpy(name_buffer, argv[0])` and
/// `strcpy(saved_filename, filename)` copied raw user strings into fixed
/// kernel-stack buffers, giving kernel-stack overflow with EIP control
/// (demonstrated in the #190 PoC). The name is now truncated like Linux
/// truncates comm, and over-long filenames fail with ENAMETOOLONG.
/// The test re-executes itself with crafted argv[0] lengths and checks, via
/// /proc/<pid>/cmdline (which reports task->name), that the name landed
/// bounded and NUL-terminated at the exact boundaries.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

/// The longest name sys_execve keeps after truncation: NAME_MAX - 1
/// characters plus the terminator.
#define MAX_NAME_CHARS 254

/// Path of this test binary inside the guest.
#define SELF_PATH "/bin/tests/t_execve_bounds"

/// A filename longer than any real path, built once.
static char long_filename[4096 + 1];

/// @brief Builds a string of `len` identical characters, NUL-terminated.
/// @param buf the destination, at least len + 1 bytes.
/// @param len the length of the filler run.
/// @param c the filler character.
static void fill_string(char *buf, size_t len, char c)
{
    memset(buf, c, len);
    buf[len] = '\0';
}

/// @brief Reads the whole content of a proc file.
/// @param path the file to read.
/// @param buf the destination buffer.
/// @param bufsize the size of buf.
/// @return the number of bytes read, or -1 on failure.
static int read_all(const char *path, char *buf, size_t bufsize)
{
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_execve_bounds] open %s: %s", path, strerror(errno));
        return -1;
    }
    unsigned total = 0;
    for (;;) {
        ssize_t n = read(fd, buf + total, bufsize - total - 1);
        if (n < 0) {
            syslog(LOG_ERR, "[t_execve_bounds] read %s: %s", path, strerror(errno));
            close(fd);
            return -1;
        }
        if (n == 0) {
            break;
        }
        total += (unsigned)n;
    }
    buf[total] = '\0';
    close(fd);
    return (int)total;
}

/// @brief Phase 2: reached through a fresh execve with a crafted argv[0].
/// Verifies that the kernel kept the task name bounded and terminated.
/// @param argv0 the argv[0] this image was started with.
/// @return 0 on success, 1 on failure.
static int phase2(const char *argv0)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", getpid());
    char cmdline[512];
    int n = read_all(path, cmdline, sizeof(cmdline));
    if (n < 0) {
        return 1;
    }
    size_t original = strlen(argv0);
    size_t expected = (original > MAX_NAME_CHARS) ? MAX_NAME_CHARS : original;
    if ((size_t)n != expected) {
        syslog(
            LOG_ERR, "[t_execve_bounds] cmdline is %d chars, expected %u "
                     "(argv[0] was %u chars)",
            n, (unsigned)expected, (unsigned)original);
        return 1;
    }
    for (size_t i = 0; i < expected; ++i) {
        if (cmdline[i] != argv0[i]) {
            syslog(LOG_ERR, "[t_execve_bounds] cmdline differs from argv[0] at %u", (unsigned)i);
            return 1;
        }
    }
    return 0;
}

/// @brief Re-executes this binary with an argv[0] of the given length and
/// requires the child (phase 2) to verify the truncated name.
/// @param len the length of the crafted argv[0].
/// @return 0 on success, -1 on failure.
static int check_name_boundary(size_t len)
{
    char name[512];
    if (len >= sizeof(name)) {
        return -1;
    }
    fill_string(name, len, 'A');
    char *argv_child[3] = {name, "phase2", NULL};
    char *envp_none[1]  = {NULL};

    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "[t_execve_bounds] fork: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        execve(SELF_PATH, argv_child, envp_none);
        // Getting here means the execve itself failed.
        syslog(LOG_ERR, "[t_execve_bounds] self-exec with %u-char argv[0]: %s", (unsigned)len, strerror(errno));
        exit(99);
    }
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        syslog(LOG_ERR, "[t_execve_bounds] waitpid: %s", strerror(errno));
        return -1;
    }
    if (WIFSIGNALED(status)) {
        syslog(LOG_ERR, "[t_execve_bounds] child with %u-char argv[0] died by signal %d", (unsigned)len, WTERMSIG(status));
        return -1;
    }
    if (!WIFEXITED(status) || (WEXITSTATUS(status) != 0)) {
        syslog(
            LOG_ERR, "[t_execve_bounds] child with %u-char argv[0] exited badly (%d)",
            (unsigned)len, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return -1;
    }
    return 0;
}

/// @brief A filename that does not fit PATH_MAX must fail with
/// ENAMETOOLONG, not silently exec a truncated path.
/// @return 0 on success, -1 on failure.
static int check_filename_too_long(void)
{
    fill_string(long_filename, 4096, 'x');
    char *argv_ok[2]  = {"t_execve_bounds", NULL};
    char *envp_none[1] = {NULL};
    errno              = 0;
    if (execve(long_filename, argv_ok, envp_none) != -1) {
        syslog(LOG_ERR, "[t_execve_bounds] execve of a 4096-char filename unexpectedly succeeded");
        return -1;
    }
    if (errno != ENAMETOOLONG) {
        syslog(LOG_ERR, "[t_execve_bounds] 4096-char filename: expected ENAMETOOLONG, got %s", strerror(errno));
        return -1;
    }
    return 0;
}

/// @brief A filename of exactly PATH_MAX - 1 characters fits: it must be
/// walked safely and fail with ENOENT (it cannot exist), not with a crash
/// or a memory error.
/// @return 0 on success, -1 on failure.
static int check_filename_longest_valid(void)
{
    fill_string(long_filename, 4095, 'y');
    char *argv_ok[2]  = {"t_execve_bounds", NULL};
    char *envp_none[1] = {NULL};
    errno              = 0;
    if (execve(long_filename, argv_ok, envp_none) != -1) {
        syslog(LOG_ERR, "[t_execve_bounds] execve of a 4095-char filename unexpectedly succeeded");
        return -1;
    }
    if (errno != ENOENT) {
        syslog(LOG_ERR, "[t_execve_bounds] 4095-char filename: expected ENOENT, got %s", strerror(errno));
        return -1;
    }
    return 0;
}

/// @brief A plain execve must still succeed end to end.
/// @return 0 on success, -1 on failure.
static int check_ordinary_execve(void)
{
    char *argv_echo[3] = {"echo", "t_execve_bounds: ordinary exec", NULL};
    char *envp_none[1] = {NULL};
    pid_t pid          = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "[t_execve_bounds] fork: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        execve("/bin/echo", argv_echo, envp_none);
        syslog(LOG_ERR, "[t_execve_bounds] execve /bin/echo: %s", strerror(errno));
        exit(99);
    }
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        syslog(LOG_ERR, "[t_execve_bounds] waitpid: %s", strerror(errno));
        return -1;
    }
    if (!WIFEXITED(status) || (WEXITSTATUS(status) != 0)) {
        syslog(LOG_ERR, "[t_execve_bounds] ordinary execve child failed");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    // Phase 2: this image was started by check_name_boundary with a crafted
    // argv[0]; verify the kernel-side name now.
    if ((argc == 2) && (strcmp(argv[1], "phase2") == 0)) {
        return phase2(argv[0]);
    }

    // The task name must survive every boundary length, including the ones
    // past NAME_MAX (truncated) and past the old 100-char field.
    const size_t lengths[] = {1, 100, 150, 254, 255, 300};
    for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        if (check_name_boundary(lengths[i]) < 0) {
            return EXIT_FAILURE;
        }
    }

    if (check_filename_too_long() < 0) {
        return EXIT_FAILURE;
    }
    if (check_filename_longest_valid() < 0) {
        return EXIT_FAILURE;
    }
    if (check_ordinary_execve() < 0) {
        return EXIT_FAILURE;
    }
    syslog(LOG_INFO, "[t_execve_bounds] all execve string-bound checks passed");
    return EXIT_SUCCESS;
}
