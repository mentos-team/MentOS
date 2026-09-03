/// @file t_fork.c
/// @brief Test the fork syscall.
/// @details This program tests the `fork` system call by creating child
/// processes and having them execute in a loop until a specified number of
/// processes is reached. Each process waits for its child to finish before
/// exiting.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    pid_t pid;

    // Fork a new process
    pid = fork();

    if (pid < 0) {
        // Error in forking
        syslog(LOG_ERR, "[t_fork] fork: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        // Child process
        syslog(LOG_INFO, "[t_fork] Child process: PID = %d, Parent PID = %d\n", getpid(), getppid());
        // Simulate some work in the child
        sleep(1);
        syslog(LOG_INFO, "[t_fork] Child process exiting successfully.\n");
        exit(EXIT_SUCCESS);
    } else {
        // Parent process
        syslog(LOG_INFO, "[t_fork] Parent process: PID = %d, Child PID = %d\n", getpid(), pid);

        // Wait for the child process to complete
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            syslog(LOG_ERR, "[t_fork] waitpid: %s", strerror(errno));
            return EXIT_FAILURE;
        }

        // Check if the child exited normally
        if (WIFEXITED(status)) {
            syslog(LOG_INFO, "[t_fork] Parent process: Child exited with status %d.\n", WEXITSTATUS(status));
            return EXIT_SUCCESS;
        }
        syslog(LOG_INFO, "[t_fork] Parent process: Child did not exit normally.\n");
        return EXIT_FAILURE;
    }
}
