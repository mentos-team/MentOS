/// @file t_stopcont.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

/// @brief Signal handler for SIGCONT.
/// @param sig The signal number.
void handle_signal(int sig)
{
    if (sig == SIGCONT) {
        printf("Received SIGCONT, continuing execution...\n");
    }
}

int main(int argc, char *argv[])
{
    pid_t pid = fork();

    if (pid < 0) {
        // Error handling for fork failure.
        perror("fork failed");
        exit(EXIT_FAILURE);

    } else if (pid == 0) { // Child process.

        // Error handling for signal setup failure.
        if (signal(SIGCONT, handle_signal) == SIG_ERR) {
            perror("signal setup failed");
            exit(EXIT_FAILURE);
        }

        printf("Child process (PID: %d) started.\n", getpid());

        while (1) {
            printf("Child process running...\n");
            sleep(1);
        }

    } else { // Parent process.

        // Let the child process run for a bit.
        sleep(3);
        if (kill(pid, SIGSTOP) == -1) {
            perror("failed to send SIGSTOP");
            exit(EXIT_FAILURE);
        }
        printf("Parent sending SIGSTOP to child (PID: %d).\n", pid);

        // Wait for a bit before continuing the child process.
        sleep(3);
        if (kill(pid, SIGCONT) == -1) {
            perror("failed to send SIGCONT");
            exit(EXIT_FAILURE);
        }
        printf("Parent sending SIGCONT to child (PID: %d).\n", pid);

        // Wait for a bit before terminating the child process.
        sleep(3);
        if (kill(pid, SIGTERM) == -1) {
            perror("failed to send SIGTERM");
            exit(EXIT_FAILURE);
        }
        printf("Parent sending SIGTERM to child (PID: %d).\n", pid);

        // Wait for the child process to finish.
        if (wait(NULL) == -1) {
            perror("wait failed");
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}
