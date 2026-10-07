/// @file t_resumable_context.c
/// @brief Deterministic smoke test for resumable kernel continuations.
/// @details Exercises blocking pipe reads, waitpid, and nanosleep in short
///          parent/child cycles. The messages are also useful when inspecting
///          a serial trace: each successful phase proves that execution
///          resumed at the call site after a kernel-side schedule().
/// @copyright (c) 2024-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define CYCLES 8

static int child_status(pid_t child)
{
    int status = 0;
    if (waitpid(child, &status, 0) != child) {
        syslog(LOG_ERR, "[t_resumable_context] waitpid failed: %d", errno);
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        syslog(LOG_ERR, "[t_resumable_context] child status=0x%x", status);
        return 1;
    }
    return 0;
}

static int pipe_read_cycle(int cycle)
{
    int fds[2];
    if (pipe(fds) < 0) {
        syslog(LOG_ERR, "[t_resumable_context] pipe cycle %d: %d", cycle, errno);
        return 1;
    }

    pid_t child = fork();
    if (child < 0) {
        close(fds[0]);
        close(fds[1]);
        return 1;
    }
    if (child == 0) {
        char byte = 0;
        close(fds[1]);
        syslog(LOG_INFO, "[t_resumable_context] cycle %d: read blocks", cycle);
        ssize_t got = read(fds[0], &byte, sizeof(byte));
        close(fds[0]);
        if (got != 1 || byte != (char)cycle) {
            exit(2);
        }
        syslog(LOG_INFO, "[t_resumable_context] cycle %d: read resumed", cycle);
        exit(0);
    }

    close(fds[0]);
    timespec_t delay = {0, 20000000};
    if (nanosleep(&delay, NULL) < 0) {
        close(fds[1]);
        return 1;
    }
    char byte = (char)cycle;
    if (write(fds[1], &byte, sizeof(byte)) != 1) {
        close(fds[1]);
        return 1;
    }
    close(fds[1]);
    return child_status(child);
}

static int waitpid_cycle(int cycle)
{
    pid_t child = fork();
    if (child < 0) {
        return 1;
    }
    if (child == 0) {
        timespec_t delay = {0, 30000000};
        if (nanosleep(&delay, NULL) < 0) {
            exit(2);
        }
        exit(0);
    }

    syslog(LOG_INFO, "[t_resumable_context] cycle %d: waitpid blocks", cycle);
    int result = child_status(child);
    if (result == 0) {
        syslog(LOG_INFO, "[t_resumable_context] cycle %d: waitpid resumed", cycle);
    }
    return result;
}

int main(void)
{
    for (int cycle = 0; cycle < CYCLES; ++cycle) {
        if (pipe_read_cycle(cycle) != 0 || waitpid_cycle(cycle) != 0) {
            syslog(LOG_ERR, "[t_resumable_context] failed at cycle %d", cycle);
            return 1;
        }
    }
    syslog(LOG_INFO, "[t_resumable_context] all continuation phases passed");
    return 0;
}
