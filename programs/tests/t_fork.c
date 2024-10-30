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
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    // Check if the correct number of arguments is provided.
    if (argc != 2) {
        fprintf(STDERR_FILENO, "Usage: %s <number_of_processes>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Convert the argument to an integer.
    char *ptr;
    int N = strtol(argv[1], &ptr, 10);
    // Check if the conversion was successful and the number is non-negative.
    if (*ptr != '\0' || N < 0) {
        fprintf(STDERR_FILENO, "Invalid number: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    pid_t cpid, mypid;

    while (1) {
        mypid = getpid(); // Get the current process ID.
        if (N > 0) {
            // Fork a new process.
            if ((cpid = fork()) == 0) {
                // In the child process, decrement N and continue the loop.
                N -= 1;
                continue;
            }
            // In the parent process, print a message and wait for the child to finish.
            printf("I'm %d and I will wait for %d (N = %d)!\n", mypid, cpid, N);
            while (wait(NULL) != -1) continue;
            printf("I'm %d and I waited for %d (N = %d)!\n", mypid, cpid, N);
        } else {
            // If N is 0, print a message and exit the loop.
            printf("I'm %d and I will not wait!\n", mypid);
        }
        break;
    }

    return EXIT_SUCCESS;
}
