/// @file t_siginfo.c
/// @brief Demonstrates handling SIGFPE with siginfo structure to get detailed
/// signal information.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strerror.h>
#include <sys/wait.h>
#include <time.h>

/// @brief Signal handler for SIGFPE that uses siginfo_t to get more information
/// about the signal.
/// @param sig Signal number.
/// @param siginfo Pointer to siginfo_t structure containing detailed
/// information about the signal.
void sig_handler_info(int sig, siginfo_t *siginfo)
{
    printf("handler(%d, %p) : Starting handler.\n", sig, siginfo);

    // Check if the received signal is SIGFPE.
    if (sig == SIGFPE) {
        printf("handler(%d, %p) : Correct signal.\n", sig, siginfo);

        // Print additional information from the siginfo structure.
        printf("handler(%d, %p) : Code : %d\n", sig, siginfo, siginfo->si_code);
        printf("handler(%d, %p) : Exiting\n", sig, siginfo);

        // Exit the process after handling the signal.
        exit(EXIT_SUCCESS);
    }

    // Handle unexpected signals.
    printf("handler(%d, %p) : Wrong signal.\n", sig, siginfo);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    sigaction_t action;

    // Initialize the sigaction structure with zeros.
    memset(&action, 0, sizeof(action));

    // Set the handler function and indicate that we want detailed information
    // (SA_SIGINFO).
    action.sa_handler = (sighandler_t)sig_handler_info;
    action.sa_flags   = SA_SIGINFO;

    // Attempt to set the signal handler for SIGFPE.
    if (sigaction(SIGFPE, &action, NULL) == -1) {
        // Print error message if sigaction fails.
        printf("Failed to set signal handler (%s).\n", SIGFPE, strerror(errno));
        return 1;
    }

    printf("Diving by zero (unrecoverable)...\n");

    // Perform a division that will eventually cause a divide-by-zero error to
    // trigger SIGFPE.
    int d = 1, e = 1;

    // Enter an infinite loop to progressively decrement e and eventually
    // trigger SIGFPE.
    while (1) {
        // Check to prevent division by zero for safety in other environments.
        if (e == 0) {
            fprintf(stderr, "Attempt to divide by zero.\n");
            break;
        }
        d /= e;
        e -= 1;
    }

    // This line will not be reached if SIGFPE is triggered.
    printf("d: %d, e: %d\n", d, e);

    return EXIT_SUCCESS;
}
