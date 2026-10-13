/// @file runtests.c
/// @brief
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <io/port_io.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

/// Debug exit port for QEMU isa-debug-exit device (default iobase=0x501)
/// Exit code encoding: host_exit = (guest_value << 1) | 1
#define DEBUG_EXIT_PORT 0x501
/// Second serial port for QEMU.
#define SERIAL_COM2   0x02F8

static char *all_tests[] = {
    "t_aab_pipe",
    "t_abort",
    "t_alarm",
    // "t_big_write",
    "t_chdir",
    "t_creat",
    "t_creat_errno",
    "t_nice",
    "t_dir_block_boundary",
    "t_dir_entries",
    "t_dup",
    "t_environ",
    "t_exec",
    "t_execve_bounds",
    "t_execve_bigargv",
    "t_userfault",
    "t_path_bounds",
    "t_execve_fail",
    "t_elf_short",
    "t_elf_malformed",
    "t_exit",
    "t_fflush",
    "t_file_blocks",
    "t_fhs",
    "t_file_blocks_free",
    "t_trunc_content",
    "t_write_aligned",
    "t_font",
    "t_fork",
    "t_getcwd",
    "t_gid",
    "t_groups",
    "t_grp",
    "t_hashmap",
    "t_itimer",
    "t_kill",
    "t_kill_perm",
    "t_list",
    "t_mem",
    "t_mkdir",
    "t_syslog_format",
    "t_printf_length",
    "t_rmdir_dotfiles",
    "t_stdint",
    "t_mount_boundary",
    "t_name_max",
    // Skipped on purpose: filling the image to make the allocation fail takes
    // about two minutes of guest time, too much for every CI run. Run it by
    // hand when touching the ext2 allocation paths (#264).
    // "t_nospace",
    "t_msgget",
    "t_ndtree",
    "t_periodic1",
    "t_pipe_eof_data",
    "t_proc_feedback",
    "t_procfs_read",
    "t_periodic2",
    "t_periodic3",
    "t_pipe_blocking",
    "t_resumable_context",
    "t_resumable_timer_irq",
    "t_resumable_timer_fanout",
    "t_fpu_context",
    "t_resumable_signals",
    "t_pipe_non_blocking",
    "t_pwd",
    "t_schedfb",
    "t_semflg",
    "t_semget",
    "t_semop",
    "t_ipc_lifetime",
    "t_ipc_key_race",
    "t_shm",
    "t_shmget",
    "t_sigaction",
    "t_sigfpe",
    "t_siginfo",
    "t_sigmask",
    "t_sigusr",
    "t_shebang",
    "t_sync",
    "t_sleep",
    "t_spwd",
    "t_stopcont",
    "t_syscall_ni",
    "t_syslog",
    "t_symlink",
    "t_time",
    "t_userptr",
    "t_wifsignaled",
    "t_write_read",
    "t_faultinj",
    "t_indirect_map",
    "t_meta_rmw",
    "t_namei_io",
};

static char **tests = &all_tests[0];
static int testsc   = count_of(all_tests);

static char buf[4096];
static char *bufpos = buf;

static int test_out_fd;
static int test_err_fd;

/// Maximum amount of child output retained for one TAP diagnostic.
#define TEST_OUTPUT_LIMIT (16 * 1024)
/// Temporary file used to capture one child test at a time.
#define TEST_OUTPUT_PATH "/home/user/.runtests-output"

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
        while ((*s) != 0) {
            outportb(SERIAL_COM2, *s++);
        }
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

    // Keep the otherwise opaque exit 127 diagnostic: the test never started
    // when execvp returns, so errno is the only useful failure context.
    fprintf(STDERR_FILENO, "execvp(%s): %s\n", test_abspath, strerror(errno));
}

/// @brief Emit captured child output as TAP diagnostic comments.
/// @param fd File descriptor containing the child's stdout and stderr.
static void emit_test_output(int fd)
{
    char line[256];
    char chunk[256];
    size_t line_len = 0;
    size_t captured = 0;
    int truncated = 0;

    if (lseek(fd, 0, SEEK_SET) < 0) {
        test_out("# unable to rewind captured output: %s", strerror(errno));
        return;
    }

    for (;;) {
        ssize_t count = read(fd, chunk, sizeof(chunk));
        if (count <= 0) {
            break;
        }

        for (ssize_t i = 0; i < count; ++i) {
            if (captured >= TEST_OUTPUT_LIMIT) {
                truncated = 1;
                continue;
            }
            captured++;

            char ch = chunk[i];
            if (ch == '\0') {
                ch = '?';
            }
            if (ch == '\n') {
                line[line_len] = '\0';
                test_out("# %s", line);
                line_len = 0;
            } else if (line_len < sizeof(line) - 1) {
                line[line_len++] = ch;
            } else {
                line[line_len] = '\0';
                test_out("# %s", line);
                line_len = 0;
            }
        }
    }

    if (line_len > 0) {
        line[line_len] = '\0';
        test_out("# %s", line);
    }
    if (truncated) {
        test_out("# ... output truncated after %d bytes", TEST_OUTPUT_LIMIT);
    }
}

static void run_test(int n, char *test_cmd_line)
{
    int output_fd = open(TEST_OUTPUT_PATH, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (output_fd < 0) {
        test_ok(n, 0, "Output capture unavailable: %s", strerror(errno));
        return;
    }
    test_out_fd = output_fd;
    test_err_fd = output_fd;

    int child = fork();
    if (child == 0) {
        exec_test(test_cmd_line);
        // If the exec returns something went wrong
        exit(127);
    }

    if (child < 0) {
        fprintf(STDERR_FILENO, "fork: %s\n", strerror(errno));
        close(output_fd);
        unlink(TEST_OUTPUT_PATH);
        exit(EXIT_FAILURE);
    }

    int status;
    waitpid(child, &status, 0);

    int success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (success) {
        test_ok(n, success, NULL);
    } else {
        if (WIFSIGNALED(status)) {
            test_ok(n, success, "Signal: %d", WTERMSIG(status));
        } else {
            test_ok(n, success, "Exit: %d", WEXITSTATUS(status));
        }
        emit_test_output(output_fd);
    }

    close(output_fd);
    unlink(TEST_OUTPUT_PATH);
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

    if (argc > 1) {
        tests  = argv + 1;
        testsc = argc - 1;
    }

    append("1..%d", testsc);
    test_out_flush();

    char *test_argv[32];
    for (int i = 0; i < testsc; i++) {
        syslog(LOG_INFO, "Running test (%2d/%2d): %s\n", i + 1, testsc, tests[i]);
        run_test(i + 1, tests[i]);
    }

    // We are running as init
    if (init) {
        // Signal success via isa-debug-exit (guest write 0x10 → host exit 33)
        outports(DEBUG_EXIT_PORT, 0x10);
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
            while (1) {
                wait(NULL);
            }
        }
    }

    return runtests_main(argc, argv);
}
