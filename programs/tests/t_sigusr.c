/// @file t_sigusr.c
/// @brief Demonstrates handling of SIGUSR1 and SIGUSR2 signals using a shared
/// signal handler. The program exits after receiving SIGUSR1 twice or SIGUSR2
/// once.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strerror.h>
#include <sys/wait.h>
#include <time.h>

/// @brief Signal handler for SIGUSR1 and SIGUSR2.
/// @param sig Signal number (should be SIGUSR1 or SIGUSR2).
void sig_handler(int sig)
{
    printf("handler(%d) : Starting handler.\n", sig);

    // Static variable to count the number of SIGUSR1 signals received.
    static int counter = 0;

    if (sig == SIGUSR1 || sig == SIGUSR2) {
        printf("handler(%d) : Correct signal. SIGUSER\n", sig);
        // Increment the counter for received signals.
        counter += 1;

        // Exit if SIGUSR1 has been received twice.
        if (counter == 2) {
            exit(EXIT_SUCCESS); // Exit program when SIGUSR1 has been received twice.
        }
    } else {
        // Handle unexpected signals.
        printf("handler(%d) : Wrong signal.\n", sig);
        exit(EXIT_FAILURE);
    }

    printf("handler(%d) : Ending handler.\n", sig);
}

int main(int argc, char *argv[])
{
    sigaction_t action;

    // Initialize sigaction structure to zero.
    memset(&action, 0, sizeof(action));
    action.sa_handler = sig_handler;

    // Set the signal handler for SIGUSR1.
    if (sigaction(SIGUSR1, &action, NULL) == -1) {
        printf("Failed to set signal handler for SIGUSR1 (%s).\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Set the signal handler for SIGUSR2.
    if (sigaction(SIGUSR2, &action, NULL) == -1) {
        printf("Failed to set signal handler for SIGUSR2 (%s).\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Send SIGUSR1 signal to the current process.
    if (kill(getpid(), SIGUSR1) == -1) {
        perror("kill SIGUSR1");
        return EXIT_FAILURE;
    }

    // Pause for a short period before sending the next signal.
    sleep(2);

    // Send SIGUSR2 signal to the current process.
    if (kill(getpid(), SIGUSR2) == -1) {
        perror("kill SIGUSR2");
        return EXIT_FAILURE;
    }

    // Infinite loop to keep the program running and waiting for signals.
    while (1) {}

    // This point will never be reached due to the infinite loop.
    return EXIT_SUCCESS;
}
