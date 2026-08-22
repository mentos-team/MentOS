/// @file t_shebang.c
/// @brief Regression tests for the shebang/interpreter path of execve (#209).
/// @details Exercises the interpreter branch of `__load_executable()`:
///          the happy path, a missing interpreter, an interpreter loop, a
///          shebang line without a trailing newline (which used to trigger a
///          double `vfs_close`), a file whose shebang read fails (sparse
///          holes, which used to index the stack buffer with a negative
///          read result), and short files (which used to be classified
///          through an uninitialized read).
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

/// Directory where the test scripts are created.
#define SCRIPT_DIR "/home/user/"

/// Path of the sparse (hole-punched) script staged inside the filesystem image.
#define SPARSE_SCRIPT SCRIPT_DIR "t_shebang_sparse.sh"

/// Creates a script file with the given content and mode.
/// @param name The file name inside SCRIPT_DIR.
/// @param content The file content.
/// @param len The length of the content.
/// @param mode The permissions of the file.
/// @return 0 on success, -1 on error.
static int create_script(const char *name, const char *content, size_t len, mode_t mode)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), SCRIPT_DIR "%s", name);
    unlink(path);
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_shebang] open(%s): %s", path, strerror(errno));
        return -1;
    }
    if (write(fd, content, len) != (ssize_t)len) {
        syslog(LOG_ERR, "[t_shebang] write(%s): %s", path, strerror(errno));
        close(fd);
        unlink(path);
        return -1;
    }
    close(fd);
    return 0;
}

/// Forks, execs the given script and waits for the child.
/// @return 0 if execve succeeded and the interpreter exited with status 0,
///         the errno if execve returned cleanly in the child,
///         -1 if the child was killed by a signal,
///         -2 on fork/waitpid failure.
static int run_script(const char *path)
{
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "[t_shebang] fork: %s", strerror(errno));
        return -2;
    }
    if (pid == 0) {
        char *argv[] = { (char *)path, NULL };
        char *envp[] = { NULL };
        execve(path, argv, envp);
        // execve returned, report the errno through the exit status.
        exit(errno);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        syslog(LOG_ERR, "[t_shebang] waitpid: %s", strerror(errno));
        return -2;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

/// Checks the outcome of an exec that is expected to fail after the address
/// space of the child has already been torn down. Until failed execve becomes
/// transactional (#208), such a child is killed by SIGSEGV instead of
/// receiving the errno; both outcomes are accepted, success is not.
/// @param res The result of run_script().
/// @param expected_errno The errno expected once #208 is fixed.
/// @param name The name of the test case (for logging).
/// @return 1 if the outcome is acceptable, 0 otherwise.
static int check_post_teardown_failure(int res, int expected_errno, const char *name)
{
    if (res == 0) {
        syslog(LOG_ERR, "[t_shebang] %s: execve unexpectedly succeeded", name);
        return 0;
    }
    if (res == -1) {
        // The child died from the destroyed-image SIGSEGV (#208).
        syslog(LOG_NOTICE, "[t_shebang] %s: child signalled (#208 behavior)", name);
        return 1;
    }
    if (res == expected_errno) {
        return 1;
    }
    syslog(LOG_ERR, "[t_shebang] %s: expected errno %d, got %d", name, expected_errno, res);
    return 0;
}

int main(int argc, char *argv[])
{
    int failures = 0;

    // ------------------------------------------------------------------
    // 1) A valid script: the interpreter must be executed successfully.
    //    Note: the whole line after `#!` is used as the interpreter path,
    //    so no interpreter arguments can be given.
    // ------------------------------------------------------------------
    if (create_script("t_shebang_ok.sh", "#!/bin/echo\n", 12, 0755) < 0) {
        return EXIT_FAILURE;
    }
    int res = run_script(SCRIPT_DIR "t_shebang_ok.sh");
    if (res != 0) {
        syslog(LOG_ERR, "[t_shebang] valid script: exec failed (%d)", res);
        ++failures;
    }
    unlink(SCRIPT_DIR "t_shebang_ok.sh");

    // ------------------------------------------------------------------
    // 2) A script without the execute bit must be refused with EACCES.
    // ------------------------------------------------------------------
    if (create_script("t_shebang_noexec.sh", "#!/bin/echo nope\n", 17, 0644) < 0) {
        return EXIT_FAILURE;
    }
    res = run_script(SCRIPT_DIR "t_shebang_noexec.sh");
    if (res != EACCES) {
        syslog(LOG_ERR, "[t_shebang] non-executable: expected EACCES, got %d", res);
        ++failures;
    }
    unlink(SCRIPT_DIR "t_shebang_noexec.sh");

    // ------------------------------------------------------------------
    // 3) An empty file must be refused with ENOEXEC, not classified as a
    //    script through an uninitialized read.
    // ------------------------------------------------------------------
    if (create_script("t_shebang_empty.sh", "", 0, 0755) < 0) {
        return EXIT_FAILURE;
    }
    res = run_script(SCRIPT_DIR "t_shebang_empty.sh");
    if (res != ENOEXEC) {
        syslog(LOG_ERR, "[t_shebang] empty script: expected ENOEXEC, got %d", res);
        ++failures;
    }
    unlink(SCRIPT_DIR "t_shebang_empty.sh");

    // ------------------------------------------------------------------
    // 4) A one-byte file: a short read must not be mistaken for a shebang.
    // ------------------------------------------------------------------
    if (create_script("t_shebang_short.sh", "#", 1, 0755) < 0) {
        return EXIT_FAILURE;
    }
    res = run_script(SCRIPT_DIR "t_shebang_short.sh");
    if (res != ENOEXEC) {
        syslog(LOG_ERR, "[t_shebang] short script: expected ENOEXEC, got %d", res);
        ++failures;
    }
    unlink(SCRIPT_DIR "t_shebang_short.sh");

    // ------------------------------------------------------------------
    // 5) A shebang line without a trailing newline must fail cleanly; it
    //    used to `vfs_close` the script file twice (use-after-free).
    // ------------------------------------------------------------------
    if (create_script("t_shebang_noeol.sh", "#!/bin/echo", 11, 0755) < 0) {
        return EXIT_FAILURE;
    }
    res = run_script(SCRIPT_DIR "t_shebang_noeol.sh");
    if (!check_post_teardown_failure(res, ENAMETOOLONG, "no-newline script")) {
        ++failures;
    }
    unlink(SCRIPT_DIR "t_shebang_noeol.sh");

    // ------------------------------------------------------------------
    // 6) A script whose interpreter does not exist must fail cleanly; the
    //    duplicated interpreter path used to leak on this path.
    // ------------------------------------------------------------------
    if (create_script("t_shebang_missing.sh", "#!/no/such/interpreter\n", 24, 0755) < 0) {
        return EXIT_FAILURE;
    }
    res = run_script(SCRIPT_DIR "t_shebang_missing.sh");
    if (!check_post_teardown_failure(res, ENOENT, "missing interpreter")) {
        ++failures;
    }
    unlink(SCRIPT_DIR "t_shebang_missing.sh");

    // ------------------------------------------------------------------
    // 7) A script interpreting itself must fail with ELOOP.
    // ------------------------------------------------------------------
    if (create_script("t_shebang_loop.sh", "#!/home/user/t_shebang_loop.sh\n", 31, 0755) < 0) {
        return EXIT_FAILURE;
    }
    res = run_script(SCRIPT_DIR "t_shebang_loop.sh");
    if (!check_post_teardown_failure(res, ELOOP, "interpreter loop")) {
        ++failures;
    }
    unlink(SCRIPT_DIR "t_shebang_loop.sh");

    // ------------------------------------------------------------------
    // 8) A script whose shebang read fails (the staged file has a sparse
    //    hole inside the first 4098 bytes, so the read at offset 2 fails;
    //    this depends on the current ext2 sparse-hole behavior of #192).
    //    The read result used to be used as a buffer index unchecked.
    // ------------------------------------------------------------------
    res = run_script(SPARSE_SCRIPT);
    if (!check_post_teardown_failure(res, EIO, "sparse script")) {
        ++failures;
    }

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
