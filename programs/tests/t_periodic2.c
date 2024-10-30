/// @file t_periodic2.c
/// @brief Test program for periodic scheduling.
/// @details This program sets periodic scheduling parameters for the current process
/// and periodically prints a counter. It demonstrates the use of `sched_getparam`,
/// `sched_setparam`, and `waitperiod` functions for managing periodic tasks in a real-time system.
/// The program runs until the counter reaches 10 or an error occurs in `waitperiod`.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <sched.h>
#include <unistd.h>
#include <strerror.h>
#include <stdlib.h>

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
    param.sched_priority = 1; // Set priority (example value, adjust as needed).
    param.period = 4000;      // Set period to 4000 ms.
    param.deadline = 4000;    // Set deadline to 4000 ms.
    param.is_periodic = 1;    // Set as periodic task.

    // Set modified scheduling parameters.
    if (sched_setparam(cpid, &param) == -1) {
        fprintf(STDERR_FILENO, "Failed to set scheduling parameters: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    int counter = 0;

    // Periodically print the counter.
    while (1) {
        if (++counter == 10) {
            break;
        }
        printf("[periodic2] counter: %d\n", counter);

        // Wait for the next period.
        if (waitperiod() == -1) {
            fprintf(STDERR_FILENO, "[%s] Error in waitperiod: %s\n", argv[0], strerror(errno));
            break;
        }
    }

    return EXIT_SUCCESS;
}
