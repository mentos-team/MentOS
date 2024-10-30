/// @file runtests.c
/// @brief
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include <io/port_io.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"         // Include kernel log levels.
#define __DEBUG_HEADER__ "[TRUNS ]"    ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_INFO ///< Set log level.
#include "io/debug.h"                  // Include debugging functions.

#define SHUTDOWN_PORT 0x604
/// Second serial port for QEMU.
#define SERIAL_COM2 0x02F8

static char *all_tests[] = {
    "t_exit",
    "t_abort",
    "t_alarm",
    "t_chdir",
    "t_time",
    /* "t_big_write", */
    "t_creat",
    "t_dup",
    "t_exec execl",
    "t_exec execlp",
    "t_exec execle",
    "t_exec execlpe",
    "t_exec execv",
    "t_exec execvp",
    "t_exec execve",
    "t_exec execvpe",
    "t_fork 10",
    "t_gid",
    "t_grp",
    "t_groups",
    "t_itimer",
    "t_kill",
    /* "t_mem", */
    "t_mkdir",
    "t_msgget",
    /* "t_periodic1", */
    /* "t_periodic2", */
    /* "t_periodic3", */
    "t_pwd",
    "t_schedfb",
    "t_semflg",
    "t_semget",
    "t_semop",
    "t_setenv",
    "t_shmget",
    /* "t_shm_read", */
    /* "t_shm_write", */
    "t_sigaction",
    "t_sigfpe",
    "t_siginfo",
    "t_sigmask",
    "t_sigusr",
    "t_sleep",
    "t_stopcont",
    "t_write_read",
};

static char **tests = &all_tests[0];
static int testsc   = count_of(all_tests);

static char buf[4096];
static char *bufpos = buf;

static int test_out_fd;
static int test_err_fd;

static int init;

#define append(...)                             \
    do {                                        \
        bufpos += sprintf(bufpos, __VA_ARGS__); \
        if (bufpos >= buf + sizeof(buf)) {      \
            return -1;                          \
        }                                       \
    } while (0);

static int test_out_flush(void)
{
    int ret = 0;
    if (!init) {
        ret = printf("%s\n", buf);
    } else {
        char *s = buf;
        while ((*s) != 0)
            outportb(SERIAL_COM2, *s++);
        outportb(SERIAL_COM2, '\n');
    }
    bufpos  = buf;
    *bufpos = 0;
    return ret;
}

static int test_out(const char *restrict format, ...)
{
    va_list ap;
    va_start(ap, format);
    bufpos += vsprintf(bufpos, format, ap);
    if (bufpos >= buf + sizeof(buf)) {
        return -1;
    }
    va_end(ap);

    return test_out_flush();
}

static int test_ok(int test, int success, const char *restrict format, ...)
{
    if (!success) {
        append("not ");
    }
    append("ok %2d - %s", test, tests[test - 1]);
    if (format) {
        append(": ");
        va_list ap;
        va_start(ap, format);
        bufpos += vsprintf(bufpos, format, ap);
        if (bufpos >= buf + sizeof(buf)) {
            return -1;
        }
        va_end(ap);
    }

    return test_out_flush();
}

static void exec_test(char *test_cmd_line)
{
    // Setup the childs stdout, stderr streams
    close(STDOUT_FILENO);
    dup(test_out_fd);
    close(STDERR_FILENO);
    dup(test_err_fd);

    // Build up the test argv vector
    char *test_argv[32];
    char *arg    = strtok(test_cmd_line, " ");
    test_argv[0] = arg;

    int t_argc = 1;
    while ((arg = strtok(NULL, " "))) {
        if (t_argc >= sizeof(test_argv) / sizeof(test_argv[0])) {
            exit(126);
        }
        test_argv[t_argc] = arg;
        t_argc++;
    }

    test_argv[t_argc] = NULL;

    char test_abspath[PATH_MAX];
    sprintf(test_abspath, "/bin/tests/%s", test_argv[0]);
    execvp(test_abspath, test_argv);
}

static void run_test(int n, char *test_cmd_line)
{
    int child = fork();
    if (child == 0) {
        exec_test(test_cmd_line);
        // If the exec returns something went wrong
        exit(127);
    }

    if (child < 0) {
        fprintf(STDERR_FILENO, "fork: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int status;
    waitpid(child, &status, 0);

    int success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (success) {
        test_ok(n, success, NULL);
    } else {
        if (WIFSIGNALED(status)) {
            test_ok(n, success, "Signal: %d", WSTOPSIG(status));
        } else {
            test_ok(n, success, "Exit: %d", WEXITSTATUS(status));
        }
    }
}

int runtests_main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--help", 6) == 0) {
            printf("Usage: %s [--help] [TEST]...\n", argv[0]);
            printf("Run one, more, or all available tests\n");
            printf("      --help   display this help and exit\n");
            exit(EXIT_SUCCESS);
        }
    }

    // TODO: capture test output
    int devnull = open("/dev/null", O_RDONLY, 0);
    if (devnull < 0) {
        fprintf(STDERR_FILENO, "open: /dev/null: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    test_out_fd = test_err_fd = devnull;

    if (argc > 1) {
        tests  = argv + 1;
        testsc = argc - 1;
    }

    test_out("1..%d", testsc);

    char *test_argv[32];
    for (int i = 0; i < testsc; i++) {
        pr_info("Running test (%2d/%2d): %s\n", i + 1, testsc, tests[i]);
        run_test(i + 1, tests[i]);
    }

    // We are running as init
    if (init) {
        outports(SHUTDOWN_PORT, 0x2000);
    }

    return 0;
}

int main(int argc, char **argv)
{
    // Are we the init process
    init = getpid() == 1;
    if (init) {
        pid_t runtests = fork();
        if (runtests) {
            while (1) { wait(NULL); }
        }
    }

    return runtests_main(argc, argv);
}
