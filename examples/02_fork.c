/// @file 02_fork.c
/// @brief Second example: Process creation with fork()
/// @details This program demonstrates:
/// - Creating child processes with fork()
/// - Detecting parent vs child (fork returns different values)
/// - Waiting for child completion with waitpid()
/// - Process IDs (getpid, getppid)
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    printf("Parent: My PID is %d\n", getpid());

    // Fork creates a new process
    pid_t child_pid = fork();

    if (child_pid < 0) {
        // Error in forking
        perror("fork");
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        // Child process (fork returns 0 to child)
        printf("  Child: My PID is %d, parent is %d\n", getpid(), getppid());
        printf("  Child: Doing some work...\n");
        sleep(1);
        printf("  Child: Done! Exiting.\n");
        return EXIT_SUCCESS;
    } else {
        // Parent process (fork returns child's PID to parent)
        printf("Parent: Child process created with PID %d\n", child_pid);
        printf("Parent: Waiting for child to finish...\n");

        int status;
        waitpid(child_pid, &status, 0);

        if (WIFEXITED(status)) {
            printf("Parent: Child exited with status %d\n", WEXITSTATUS(status));
        }
        printf("Parent: All done!\n");
        return EXIT_SUCCESS;
    }
}
