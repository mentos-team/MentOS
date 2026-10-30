/// @file t_shell_args.c
/// @brief Tests shell positional parameters in direct and shebang execution.
/// @copyright (c) 2024-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#define SCRIPT_PATH  "/home/user/t_shell_args.sh"
#define ERREXIT_PATH "/home/user/t_shell_errexit.sh"
#define OUTPUT_PATH  "/home/user/t_shell_args.out"
#define OUTPUT_SIZE  512

static int __create_script(const char *path, const char *contents)
{
    unlink(path);
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0755);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_shell_args] open(%s): %s", path, strerror(errno));
        return -1;
    }
    size_t length = strlen(contents);
    if (write(fd, contents, length) != (ssize_t)length) {
        syslog(LOG_ERR, "[t_shell_args] write(%s): %s", path, strerror(errno));
        close(fd);
        unlink(path);
        return -1;
    }
    close(fd);
    return 0;
}

static int __read_output(char *output, size_t output_size)
{
    output[0] = '\0';
    int fd    = open(OUTPUT_PATH, O_RDONLY, 0);
    if (fd < 0) {
        // Scripts without output (the -e one) leave no file behind.
        if (errno == ENOENT) {
            return 0;
        }
        syslog(LOG_ERR, "[t_shell_args] open(%s): %s", OUTPUT_PATH, strerror(errno));
        return -1;
    }
    ssize_t count = read(fd, output, output_size - 1);
    close(fd);
    if (count < 0) {
        syslog(LOG_ERR, "[t_shell_args] read(%s): %s", OUTPUT_PATH, strerror(errno));
        return -1;
    }
    output[count] = '\0';
    return 0;
}

static int __run_script(const char *path, char *const argv[], char *output, size_t output_size, int *exit_status)
{
    output[0] = '\0';
    unlink(OUTPUT_PATH);

    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "[t_shell_args] fork: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        char *envp[] = {"USER=user", "PATH=/bin:/usr/bin", NULL};
        execve(path, argv, envp);
        exit(errno);
    }

    int status;
    if (waitpid(pid, &status, 0) != pid) {
        syslog(LOG_ERR, "[t_shell_args] waitpid: %s", strerror(errno));
        return -1;
    }
    if (!WIFEXITED(status)) {
        return -1;
    }
    *exit_status = WEXITSTATUS(status);
    return __read_output(output, output_size);
}

int main(void)
{
    int failures = 0;
    char output[OUTPUT_SIZE];
    int exit_status = -1;
    const char *script =
        "#!/bin/shell -e\n"
        "echo $0:$#:$1:$2:$3:pre$1post > " OUTPUT_PATH "\n";

    if (__create_script(SCRIPT_PATH, script) < 0) {
        return EXIT_FAILURE;
    }
    char *script_argv[] = {
        SCRIPT_PATH,
        "first value",
        "second",
        NULL,
    };
    char *direct_argv[] = {
        "/bin/shell",
        SCRIPT_PATH,
        "first value",
        "second",
        NULL,
    };
    if ((__run_script("/bin/shell", direct_argv, output, sizeof(output), &exit_status) < 0) ||
        (exit_status != 0) || strcmp(output, SCRIPT_PATH ":2:first value:second::prefirst valuepost\n")) {
        syslog(LOG_ERR, "[t_shell_args] direct invocation output/status mismatch: status=%d output='%s'", exit_status, output);
        ++failures;
    }

    if ((__run_script(SCRIPT_PATH, script_argv, output, sizeof(output), &exit_status) < 0) ||
        (exit_status != 0) || strcmp(output, SCRIPT_PATH ":2:first value:second::prefirst valuepost\n")) {
        syslog(LOG_ERR, "[t_shell_args] positional expansion output/status mismatch: status=%d output='%s'", exit_status, output);
        ++failures;
    }
    unlink(SCRIPT_PATH);

    const char *errexit_script =
        "#!/bin/shell -e\n"
        "t_shell_args_missing_command\n"
        "echo should-not-run\n";
    if (__create_script(ERREXIT_PATH, errexit_script) < 0) {
        return EXIT_FAILURE;
    }
    char *errexit_argv[] = {ERREXIT_PATH, NULL};
    exit_status          = -1;
    if ((__run_script(ERREXIT_PATH, errexit_argv, output, sizeof(output), &exit_status) < 0) ||
        (exit_status != 127) || (strstr(output, "should-not-run") != NULL)) {
        syslog(LOG_ERR, "[t_shell_args] -e did not stop after a failed command: status=%d output='%s'", exit_status, output);
        ++failures;
    }
    unlink(ERREXIT_PATH);
    unlink(OUTPUT_PATH);

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
