/// @file t_resumable_signals.c
/// @brief Exercises resumable continuations when signals and pipe closure
///        race with blocking syscalls.
/// @details The test deliberately keeps the child in kernel space while the
///          parent sends a signal or closes the last pipe reader. A passing
///          result proves that the child resumes its original kernel call
///          chain, performs the required cleanup, and reaches userspace with
///          the expected result.
/// @copyright (c) 2024-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

static volatile int signal_seen;

static void signal_handler(int signum)
{
    if (signum == SIGUSR1) {
        signal_seen = 1;
    }
}

static int install_signal_handler(void)
{
    sigaction_t action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    return sigaction(SIGUSR1, &action, NULL);
}

static int wait_for_child(pid_t child, const char *name)
{
    int status = 0;
    if (waitpid(child, &status, 0) != child) {
        syslog(LOG_ERR, "[t_resumable_signals] %s: waitpid failed (%d)", name, errno);
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        syslog(LOG_ERR, "[t_resumable_signals] %s: child status=0x%x", name, status);
        return 1;
    }
    return 0;
}

static int test_signal_interrupts_sleep(void)
{
    pid_t child = fork();
    if (child < 0) {
        return 1;
    }
    if (child == 0) {
        signal_seen = 0;
        if (install_signal_handler() < 0) {
            exit(2);
        }
        timespec_t request = {2, 0};
        int result = nanosleep(&request, NULL);
        if (!signal_seen || (result < 0 && errno != EINTR)) {
            exit(3);
        }
        exit(0);
    }

    timespec_t delay = {0, 100000000};
    nanosleep(&delay, NULL);
    if (kill(child, SIGUSR1) < 0) {
        return 1;
    }
    return wait_for_child(child, "signal+nanosleep");
}

static int test_signal_during_pipe_read(void)
{
    int fds[2];
    int ready[2];
    if (pipe(fds) < 0 || pipe(ready) < 0) {
        return 1;
    }

    pid_t child = fork();
    if (child < 0) {
        return 1;
    }
    if (child == 0) {
        close(fds[1]);
        close(ready[0]);
        signal_seen = 0;
        if (install_signal_handler() < 0) {
            exit(2);
        }
        char marker = 'R';
        if (write(ready[1], &marker, sizeof(marker)) != 1) {
            exit(2);
        }
        close(ready[1]);
        char byte = 0;
        ssize_t result = read(fds[0], &byte, sizeof(byte));
        close(fds[0]);
        if (result != 1 || byte != 'S') {
            syslog(LOG_ERR, "[t_resumable_signals] signal+pipe-read: read=%d byte=%d errno=%d", (int)result, byte, errno);
            exit(3);
        }
        // Give the timer path a chance to deliver a signal that was queued
        // while the uninterruptible pipe continuation was asleep.
        timespec_t settle = {0, 200000000};
        nanosleep(&settle, NULL);
        exit(signal_seen ? 0 : 4);
    }

    close(fds[0]);
    close(ready[1]);
    char marker = 0;
    if (read(ready[0], &marker, sizeof(marker)) != 1 || marker != 'R') {
        close(fds[1]);
        close(ready[0]);
        return 1;
    }
    close(ready[0]);
    timespec_t delay = {0, 100000000};
    nanosleep(&delay, NULL);
    if (kill(child, SIGUSR1) < 0) {
        close(fds[1]);
        return 1;
    }
    nanosleep(&delay, NULL);
    char byte = 'S';
    ssize_t written = write(fds[1], &byte, sizeof(byte));
    int failed = written != 1;
    if (failed) {
        syslog(LOG_ERR, "[t_resumable_signals] signal+pipe-read: write=%d errno=%d", (int)written, errno);
    }
    close(fds[1]);
    return failed ? 1 : wait_for_child(child, "signal+pipe-read");
}

static int test_last_reader_wakes_writer(void)
{
    int fds[2];
    if (pipe(fds) < 0) {
        return 1;
    }

    pid_t child = fork();
    if (child < 0) {
        return 1;
    }
    if (child == 0) {
        close(fds[0]);
        char payload[4096];
        memset(payload, 'W', sizeof(payload));
        ssize_t result = write(fds[1], payload, sizeof(payload));
        close(fds[1]);
        // The pipe is intentionally smaller than payload. Closing the last
        // reader must wake the continuation with a partial write or EPIPE;
        // it must never leave the child blocked indefinitely.
        if (!(result > 0 && result < (ssize_t)sizeof(payload))) {
            syslog(LOG_ERR, "[t_resumable_signals] last-reader-close: write=%d errno=%d", (int)result, errno);
            exit(2);
        }
        exit(0);
    }

    close(fds[1]);
    timespec_t delay = {0, 100000000};
    nanosleep(&delay, NULL);
    close(fds[0]);
    return wait_for_child(child, "last-reader-close");
}

int main(void)
{
    int failures = 0;
    failures += test_signal_interrupts_sleep();
    failures += test_signal_during_pipe_read();
    failures += test_last_reader_wakes_writer();
    if (failures == 0) {
        syslog(LOG_INFO, "[t_resumable_signals] all signal/pipe continuation checks passed");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_resumable_signals] %d checks failed", failures);
    return EXIT_FAILURE;
}
