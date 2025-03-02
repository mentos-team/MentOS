/// @file t_schedfb.c
/// @brief Start the scheduler feedback session by running multiple child
/// processes.
/// @details This program forks several child processes to execute the `t_alarm`
/// test. It demonstrates basic process creation and execution control using
/// `fork`, `execl`, and `wait`. Each child process runs the `t_alarm` program,
/// but will sleep, hence, they will not be scheduled immediately.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    pid_t cpid;

    printf("First test: The child processes will sleep, so they will not be "
           "scheduled immediately.\n");

    // Fork 10 child processes to run the t_alarm test.
    for (int i = 0; i < 10; ++i) {
        // Fork a new process
        if ((cpid = fork()) == -1) {
            // Fork failed, print the error and exit.
            fprintf(stderr, "Failed to fork process: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        // Child process
        if (cpid == 0) {
            // Execute the t_alarm program in the child process.
            // execl replaces the current process image with the one specified ("/bin/tests/t_alarm").
            if (execl("/bin/tests/t_alarm", "t_alarm", NULL) == -1) {
                // If execl fails, print the error and exit the child process.
                fprintf(stderr, "Failed to exec t_alarm: %s\n", strerror(errno));
                return EXIT_FAILURE;
            }
            // Child process exits after executing the command.
            return 0;
        }
    }

    // Parent process waits for all child processes to terminate.
    while (wait(NULL) != -1) {
        // Continue waiting until all child processes finish.
        continue;
    }

    // If wait returns -1 and errno is not ECHILD, print an error.
    if (errno != ECHILD) {
        fprintf(stderr, "Error occurred while waiting for child processes: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("All child processes have completed.\n");

    return EXIT_SUCCESS;
}
