/// @file t_fork.c
/// @brief Test the fork syscall.
/// @details This program tests the `fork` system call by creating child
/// processes and having them execute in a loop until a specified number of
/// processes is reached. Each process waits for its child to finish before
/// exiting.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    pid_t pid;

    // Fork a new process
    pid = fork();

    if (pid < 0) {
        // Error in forking
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        // Child process
        printf("Child process: PID = %d, Parent PID = %d\n", getpid(), getppid());
        // Simulate some work in the child
        sleep(1);
        printf("Child process exiting successfully.\n");
        exit(EXIT_SUCCESS);
    } else {
        // Parent process
        printf("Parent process: PID = %d, Child PID = %d\n", getpid(), pid);

        // Wait for the child process to complete
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            return EXIT_FAILURE;
        }

        // Check if the child exited normally
        if (WIFEXITED(status)) {
            printf("Parent process: Child exited with status %d.\n", WEXITSTATUS(status));
            return EXIT_SUCCESS;
        }
        printf("Parent process: Child did not exit normally.\n");
        return EXIT_FAILURE;
    }
}
