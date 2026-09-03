/// @file t_sigmask.c
/// @brief Demonstrates signal masking and unmasking using sigprocmask and
/// handling SIGUSR1.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

/// @brief Handler for SIGUSR1 signal.
/// @param sig Signal number (should be SIGUSR1).
void sigusr1_handler(int sig)
{
    syslog(LOG_INFO, "[t_sigmask] handler(sig: %d) : Starting handler.\n", sig);
    // Perform any necessary actions in the handler here.
    syslog(LOG_INFO, "[t_sigmask] handler(sig: %d) : Ending handler.\n", sig);
}

int main(int argc, char *argv[])
{
    int ret;
    sigaction_t action;

    // Initialize sigaction structure and set the handler for SIGUSR1.
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigusr1_handler;

    // Set the signal handler for SIGUSR1.
    if (sigaction(SIGUSR1, &action, NULL) == -1) {
        // Print error message if sigaction fails.
        syslog(LOG_ERR, "[t_sigmask] Failed to set signal handler (%d, %s).\n", SIGUSR1, strerror(errno));
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "[t_sigmask] main : Blocking signal (%d).\n", SIGUSR1);

    // Define a signal set and initialize it to empty.
    sigset_t mask;
    if (sigemptyset(&mask) == -1) { // Check for error in sigemptyset.
        syslog(LOG_ERR, "[t_sigmask] sigemptyset: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    // Add SIGUSR1 to the signal mask.
    if (sigaddset(&mask, SIGUSR1) == -1) { // Check for error in sigaddset.
        syslog(LOG_ERR, "[t_sigmask] sigaddset: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    // Block SIGUSR1 signal.
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) { // Check for error in sigprocmask.
        syslog(LOG_ERR, "[t_sigmask] sigprocmask (blocking): %s", strerror(errno));
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "[t_sigmask] main : Calling handler (%d).\n", SIGUSR1);

    // Send SIGUSR1 to the current process.
    ret = kill(getpid(), SIGUSR1);
    if (ret == -1) { // Check for error in kill.
        syslog(LOG_ERR, "[t_sigmask] kill: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "[t_sigmask] main : Returning from handler (%d): %d.\n", SIGUSR1, ret);

    syslog(LOG_INFO, "[t_sigmask] main : Unblocking signal (%d).\n", SIGUSR1);

    // Unblock SIGUSR1 signal.
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) { // Check for error in sigprocmask (unblocking).
        syslog(LOG_ERR, "[t_sigmask] sigprocmask (unblocking): %s", strerror(errno));
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "[t_sigmask] main : Calling handler (%d).\n", SIGUSR1);

    // Send SIGUSR1 to the current process again after unblocking.
    ret = kill(getpid(), SIGUSR1);
    if (ret == -1) { // Check for error in kill.
        syslog(LOG_ERR, "[t_sigmask] kill: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "[t_sigmask] main : Returning from handler (%d): %d.\n", SIGUSR1, ret);

    return EXIT_SUCCESS;
}
