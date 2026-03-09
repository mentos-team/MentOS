/// @file t_stopcont.c
/// @brief Test program for the stop and continue signals (SIGSTOP and SIGCONT).
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

/// @brief Signal handler for SIGCONT.
/// @param sig The signal number.
void handle_signal(int sig)
{
    if (sig == SIGCONT) {
        syslog(LOG_INFO, "[t_stopcont] Received SIGCONT, continuing execution...\n");
    }
}

int main(int argc, char *argv[])
{
    pid_t pid = fork();

    if (pid < 0) {
        // Error handling for fork failure.
        syslog(LOG_ERR, "[t_stopcont] fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);

    } else if (pid == 0) { // Child process.

        // Error handling for signal setup failure.
        if (signal(SIGCONT, handle_signal) == SIG_ERR) {
            syslog(LOG_ERR, "[t_stopcont] signal setup failed: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        syslog(LOG_INFO, "[t_stopcont] Child process (PID: %d) started.\n", getpid());

        // Sleep for 100 ms.
        timespec_t req = {0, 100000000};

        while (1) {
            syslog(LOG_INFO, "[t_stopcont] Child process running...\n");
            nanosleep(&req, NULL);
        }

    } else { // Parent process.

        // Sleep for 300 ms.
        timespec_t req = {0, 300000000};

        // Let the child process run for a bit.
        nanosleep(&req, NULL);
        if (kill(pid, SIGSTOP) == -1) {
            syslog(LOG_ERR, "[t_stopcont] failed to send SIGSTOP: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        syslog(LOG_INFO, "[t_stopcont] Parent sending SIGSTOP to child (PID: %d).\n", pid);

        // Wait for a bit before continuing the child process.
        nanosleep(&req, NULL);
        if (kill(pid, SIGCONT) == -1) {
            syslog(LOG_ERR, "[t_stopcont] failed to send SIGCONT: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        syslog(LOG_INFO, "[t_stopcont] Parent sending SIGCONT to child (PID: %d).\n", pid);

        // Wait for a bit before terminating the child process.
        nanosleep(&req, NULL);
        if (kill(pid, SIGTERM) == -1) {
            syslog(LOG_ERR, "[t_stopcont] failed to send SIGTERM: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        syslog(LOG_INFO, "[t_stopcont] Parent sending SIGTERM to child (PID: %d).\n", pid);

        // Wait for the child process to finish.
        if (wait(NULL) == -1) {
            syslog(LOG_ERR, "[t_stopcont] wait failed: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}
