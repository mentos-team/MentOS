/// @file t_exec.c
/// @brief Test program for the exec system call.
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
    int status;

    // Fork a new process
    pid = fork();

    if (pid < 0) {
        // Error in forking
        syslog(LOG_ERR, "[t_exec] fork: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        // Child process: Use exec to replace the child process image

        // Program to execute: /bin/echo
        // Arguments: /bin/echo "Exec test successful"
        execl("/bin/echo", "echo", "Exec test successful", NULL);

        // If exec fails, print an error and exit
        syslog(LOG_ERR, "[t_exec] execl: %s", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        // Parent process: Wait for the child process to complete
        if (waitpid(pid, &status, 0) == -1) {
            syslog(LOG_ERR, "[t_exec] waitpid: %s", strerror(errno));
            return EXIT_FAILURE;
        }

        // Check if the child terminated successfully
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            // exec worked
            return EXIT_SUCCESS;
        } // exec failed or child terminated abnormally
        return EXIT_FAILURE;
    }
}
