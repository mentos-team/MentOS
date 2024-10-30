/// @file t_periodic3.c
/// @brief Periodic task scheduling demonstration using custom scheduler
/// parameters.
/// @details This program demonstrates the use of periodic task scheduling with
/// custom parameters such as period, deadline, and periodicity. It uses
/// `sched_getparam` and `sched_setparam` to modify and apply scheduling
/// parameters for a process, and repeatedly waits for the next period using the
/// `waitperiod` function.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <strerror.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    pid_t cpid = getpid(); // Get the process ID of the current process
    sched_param_t param;   // Define a structure to hold scheduling parameters

    // Get the current scheduling parameters for this process
    if (sched_getparam(cpid, &param) == -1) {
        // If fetching parameters fails, print an error message and exit
        fprintf(stderr, "[%s] Error in sched_getparam: %s\n", argv[0], strerror(errno));
        return EXIT_FAILURE;
    }

    // Modify the scheduling parameters
    param.period      = 3000; // Set period to 3000 ms (3 seconds)
    param.deadline    = 3000; // Set deadline to 3000 ms (3 seconds)
    param.is_periodic = true; // Set the task to be periodic

    // Set the new scheduling parameters for this process
    if (sched_setparam(cpid, &param) == -1) {
        // If setting parameters fails, print an error message and exit
        fprintf(stderr, "[%s] Error in sched_setparam: %s\n", argv[0], strerror(errno));
        return EXIT_FAILURE;
    }

    int counter = 0; // Initialize a counter for the periodic loop

    // Enter a loop that runs for 10 iterations
    while (1) {
        // Increment the counter and break out of the loop after 10 iterations
        if (++counter == 10)
            break;

        // Print the current counter value
        printf("[periodic3] counter: %d\n", counter);

        // Wait for the next period and check for errors
        if (waitperiod() == -1) {
            // If waitperiod fails, print an error message and break out of the loop
            fprintf(stderr, "[%s] Error in waitperiod: %s\n", argv[0], strerror(errno));
            break;
        }
    }

    // Return success if the program completes without errors
    return EXIT_SUCCESS;
}
