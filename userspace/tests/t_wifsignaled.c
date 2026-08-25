/// @file t_wifsignaled.c
/// @brief Test program for the wait status of signal-terminated processes.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

static int check_signal_death(int sig)
{
    pid_t child = fork();
    if (child < 0) {
        syslog(LOG_ERR, "[t_wifsignaled] signal %d: fork failed: %s\n", sig, strerror(errno));
        return -1;
    }
    if (child == 0) {
        // Restore the default action and terminate ourselves with the signal.
        signal(sig, SIG_DFL);
        kill(getpid(), sig);
        // We must never get here.
        exit(42);
    }

    int status = 0;
    if (waitpid(child, &status, 0) != child) {
        syslog(LOG_ERR, "[t_wifsignaled] signal %d: waitpid failed: %s\n", sig, strerror(errno));
        return -1;
    }
    if (!WIFSIGNALED(status)) {
        syslog(LOG_ERR,
               "[t_wifsignaled] signal %d: WIFSIGNALED is false (raw status 0x%x, WIFEXITED %d, WEXITSTATUS %d)\n",
               sig,
               status,
               WIFEXITED(status),
               WEXITSTATUS(status));
        return -1;
    }
    if (WTERMSIG(status) != sig) {
        syslog(LOG_ERR, "[t_wifsignaled] signal %d: WTERMSIG is %d\n", sig, WTERMSIG(status));
        return -1;
    }
    syslog(LOG_INFO, "[t_wifsignaled] signal %2d: WIFSIGNALED true, WTERMSIG %d, WEXITSTATUS %d\n", sig, WTERMSIG(status), WEXITSTATUS(status));
    return 0;
}

int main(int argc, char *argv[])
{
    openlog("t_wifsignaled", LOG_PID | LOG_CONS, LOG_USER);

    // Signals whose default-action encoding was broken (issue #234).
    const int affected[] = { SIGQUIT, SIGILL, SIGTRAP, SIGABRT };
    // Controls: one correct explicit case, one default-path case.
    const int controls[] = { SIGSEGV, SIGTERM };

    int failed = 0;
    for (unsigned i = 0; i < sizeof(affected) / sizeof(affected[0]); ++i) {
        if (check_signal_death(affected[i]) < 0) {
            failed = 1;
        }
    }
    for (unsigned i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
        if (check_signal_death(controls[i]) < 0) {
            failed = 1;
        }
    }

    closelog();
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
