/// @file t_periodic1.c
/// @brief Test program for periodic scheduling.
/// @details This program sets periodic scheduling parameters for the current process,
/// forks two child processes to execute other programs, and periodically prints a counter.
/// It demonstrates the use of `sched_getparam`, `sched_setparam`, and `waitperiod` functions
/// for managing periodic tasks in a real-time system.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    pid_t cpid = getpid();
    struct sched_param param;

    // Get current scheduling parameters.
    if (sched_getparam(cpid, &param) == -1) {
        fprintf(STDERR_FILENO, "Failed to get scheduling parameters: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Change scheduling parameters.
    param.sched_priority = 1;    // Set priority (example value, adjust as needed).
    param.period         = 5000; // Set period to 5000 ms.
    param.deadline       = 5000; // Set deadline to 5000 ms.
    param.is_periodic    = 1;    // Set as periodic task.

    // Set modified scheduling parameters.
    if (sched_setparam(cpid, &param) == -1) {
        fprintf(STDERR_FILENO, "Failed to set scheduling parameters: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    int counter = 0;

    // Fork the first child process.
    if (fork() == 0) {
        char *_argv[] = {"/bin/tests/t_periodic2", NULL};
        execv(_argv[0], _argv);
        fprintf(STDERR_FILENO, "Failed to execute %s: %s\n", _argv[0], strerror(errno));
        return EXIT_FAILURE;
    }

    // Fork the second child process.
    if (fork() == 0) {
        char *_argv[] = {"/bin/tests/t_periodic3", NULL};
        execv(_argv[0], _argv);
        fprintf(STDERR_FILENO, "Failed to execute %s: %s\n", _argv[0], strerror(errno));
        return EXIT_FAILURE;
    }

    // Periodically print the counter.
    while (1) {
        if (++counter == 10) {
            counter = 0;
        }
        printf("[periodic1] counter %d\n", counter);

        // Wait for the next period.
        if (waitperiod() == -1) {
            fprintf(STDERR_FILENO, "[%s] Error in waitperiod: %s\n", argv[0], strerror(errno));
            break;
        }
    }

    return EXIT_SUCCESS;
}
