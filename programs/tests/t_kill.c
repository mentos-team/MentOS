/// @file t_kill.c
/// @brief Test the kill and signal handling functionality.
/// @details This program demonstrates the use of `fork`, `kill`, and signal
/// handling. It creates a child process, sets up a signal handler for `SIGUSR1`
/// in the child, and sends signals from the parent to the child. The child
/// process handles the signals and prints messages accordingly. The parent
/// process waits for the child to terminate before exiting.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/// @brief Signal handler for SIGUSR1 in the child process.
/// @param sig The signal number.
void child_sigusr1_handler(int sig)
{
    printf("handler(sig: %d) : Starting handler (pid: %d).\n", sig, getpid());
    printf("handler(sig: %d) : Ending handler (pid: %d).\n", sig, getpid());
}

int main(int argc, char *argv[])
{
    printf("main : Creating child!\n");

    // Fork the process to create a child
    pid_t cpid = fork();

    if (cpid == 0) {
        // Child process
        cpid = getpid(); // Get the child PID
        printf("I'm the child (pid: %d)!\n", cpid);

        // Set up a signal handler for SIGUSR1 in the child
        struct sigaction action;
        memset(&action, 0, sizeof(action));        // Clear the action structure
        action.sa_handler = child_sigusr1_handler; // Set handler function

        // Check if setting up the signal handler fails
        if (sigaction(SIGUSR1, &action, NULL) == -1) {
            fprintf(STDERR_FILENO, "Failed to set signal handler for SIGUSR1: %s\n", strerror(errno));
            return EXIT_FAILURE; // Return failure if handler setup fails
        }

        // Request to sleep for 100 ms.
        struct timespec req = {0, 100000000};

        // Child process loop - waiting for signals
        while (1) {
            printf("I'm the child (pid: %d): I'm waiting...\n", cpid);
            // Sleep for 100 ms.
            nanosleep(&req, NULL);
        }

    } else if (cpid > 0) {
        // Parent process
        printf("I'm the parent (pid: %d)!\n", getpid());

        // Request to sleep for 500 ms.
        struct timespec req = {0, 500000000};

        // Sleep for 500 ms.
        nanosleep(&req, NULL);

        // Send SIGUSR1 to the child process
        if (kill(cpid, SIGUSR1) == -1) {
            fprintf(STDERR_FILENO, "Failed to send SIGUSR1 to child: %s\n", strerror(errno));
            return EXIT_FAILURE; // Return failure if signal sending fails
        }

        // Wait before terminating the child process, sleep for 500 ms.
        nanosleep(&req, NULL);

        // Send SIGTERM to the child process to terminate it
        if (kill(cpid, SIGTERM) == -1) {
            fprintf(STDERR_FILENO, "Failed to send SIGTERM to child: %s\n", strerror(errno));
            return EXIT_FAILURE; // Return failure if termination fails
        }

        // Wait for the child process to terminate
        if (wait(NULL) == -1) {
            fprintf(STDERR_FILENO, "Failed to wait for child process: %s\n", strerror(errno));
            return EXIT_FAILURE; // Return failure if wait fails
        }
        printf("main : Child has terminated. End of parent process.\n");

    } else {
        // Fork failed
        fprintf(STDERR_FILENO, "Failed to fork: %s\n", strerror(errno));
        return EXIT_FAILURE; // Return failure if fork fails
    }

    return EXIT_SUCCESS; // Return success if everything runs correctly
}
