/// @file t_setenv.c
/// @brief Demonstrates the use of `setenv` to set an environment variable and
/// fork a child process that inherits the environment variable. The child
/// executes a different program (`t_getenv`) to verify the environment variable
/// is passed.
/// @copyright (c) 2014-2024
/// This file is distributed under the MIT License. See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    // Arguments to pass to the child process (t_getenv)
    char *_argv[] = { "/bin/tests/t_getenv", NULL };
    int status;

    // ========================================================================
    // Set environment variable "ENV_VAR" to "pwd0" without overwriting if it exists.
    if (setenv("ENV_VAR", "pwd0", 0) == -1) {
        perror("Failed to set environment variable `ENV_VAR`");
        return 1;
    }
    printf("Environment variable `ENV_VAR` set to `pwd0`\n");

    // ========================================================================
    // Fork a child process to execute t_getenv.
    pid_t pid = fork();
    if (pid < 0) {
        perror("Failed to fork");
        return 1;
    }

    // Child process
    if (pid == 0) {
        // Execute t_getenv to check the environment variable in the child.
        execv(_argv[0], _argv);

        // If execv returns, something went wrong.
        perror("Exec failed");
        return 1; // Terminate the child process with error if execv fails.
    }

    // ========================================================================
    // Parent process waits for the child to complete.
    if (wait(&status) == -1) {
        perror("Failed to wait for child process");
        return 1;
    }

    // Check the status of the child process
    if (WIFEXITED(status)) {
        printf("Child process exited with status: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("Child process was terminated by signal: %d\n", WTERMSIG(status));
    } else {
        printf("Child process did not exit normally.\n");
    }

    return 0;
}
