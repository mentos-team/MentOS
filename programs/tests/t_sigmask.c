/// @file t_sigmask.c
/// @brief Demonstrates signal masking and unmasking using sigprocmask and
/// handling SIGUSR1.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strerror.h>
#include <sys/wait.h>

/// @brief Handler for SIGUSR1 signal.
/// @param sig Signal number (should be SIGUSR1).
void sigusr1_handler(int sig)
{
    printf("handler(sig: %d) : Starting handler.\n", sig);
    // Perform any necessary actions in the handler here.
    printf("handler(sig: %d) : Ending handler.\n", sig);
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
        fprintf(stderr, "Failed to set signal handler (%d, %s).\n", SIGUSR1, strerror(errno));
        return EXIT_FAILURE;
    }

    printf("main : Blocking signal (%d).\n", SIGUSR1);

    // Define a signal set and initialize it to empty.
    sigset_t mask;
    if (sigemptyset(&mask) == -1) { // Check for error in sigemptyset.
        perror("sigemptyset");
        return EXIT_FAILURE;
    }

    // Add SIGUSR1 to the signal mask.
    if (sigaddset(&mask, SIGUSR1) == -1) { // Check for error in sigaddset.
        perror("sigaddset");
        return EXIT_FAILURE;
    }

    // Block SIGUSR1 signal.
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) { // Check for error in sigprocmask.
        perror("sigprocmask (blocking)");
        return EXIT_FAILURE;
    }

    printf("main : Calling handler (%d).\n", SIGUSR1);

    // Send SIGUSR1 to the current process.
    ret = kill(getpid(), SIGUSR1);
    if (ret == -1) { // Check for error in kill.
        perror("kill");
        return EXIT_FAILURE;
    }

    printf("main : Returning from handler (%d): %d.\n", SIGUSR1, ret);

    printf("main : Unblocking signal (%d).\n", SIGUSR1);

    // Unblock SIGUSR1 signal.
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) { // Check for error in sigprocmask (unblocking).
        perror("sigprocmask (unblocking)");
        return EXIT_FAILURE;
    }

    printf("main : Calling handler (%d).\n", SIGUSR1);

    // Send SIGUSR1 to the current process again after unblocking.
    ret = kill(getpid(), SIGUSR1);
    if (ret == -1) { // Check for error in kill.
        perror("kill");
        return EXIT_FAILURE;
    }

    printf("main : Returning from handler (%d): %d.\n", SIGUSR1, ret);

    return EXIT_SUCCESS;
}
