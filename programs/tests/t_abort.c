/// @file t_abort.c
/// @brief Demonstrates handling of the SIGABRT signal.
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

void sig_handler(int sig)
{
    printf("handler(%d) : Starting handler.\n", sig);
    if (sig == SIGABRT) {
        static int counter = 0;
        counter += 1;

        printf("handler(%d) : Correct signal. ABRT (%d/3)\n", sig, counter);
        if (counter < 3) {
            // Re-trigger the abort signal up to 3 times.
            abort();
        } else {
            // Exit the program after handling the signal 3 times.
            exit(EXIT_SUCCESS);
        }
    } else {
        printf("handler(%d) : Wrong signal.\n", sig);
    }
    printf("handler(%d) : Ending handler.\n", sig);
}

int main(int argc, char *argv[])
{
    sigaction_t action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sig_handler;

    // Set up the signal handler for SIGABRT.
    if (sigaction(SIGABRT, &action, NULL) == -1) {
        perror("signal setup failed");
        exit(EXIT_FAILURE);
    }

    // Trigger the SIGABRT signal.
    abort();

    // This point should never be reached.
    perror("abort() failed to terminate the process");

    return EXIT_FAILURE;
}
