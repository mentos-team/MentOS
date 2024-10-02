/// @file t_sleep.c
/// @brief Demonstrates the use of the sleep function to pause program execution
/// for a specified duration.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

int main(int argc, char *argv[])
{
    printf("Sleeping for 4 seconds... ");

    // Pause the program execution for 4 seconds. Error checking is included to
    // ensure that sleep is successful.
    if (sleep(4) != 0) {
        perror("sleep");
        return EXIT_FAILURE;
    }

    printf("COMPLETED.\n");

    return EXIT_SUCCESS;
}
