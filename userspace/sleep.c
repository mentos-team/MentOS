/// @file sleep.c
/// @brief Simple sleep program that pauses for a specified number of seconds.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    // Check if an argument is provided
    if (argc == 2) {
        // Convert the argument to an integer
        char *endptr;
        long amount = strtol(argv[1], &endptr, 10);

        // Check for conversion errors
        if (*endptr != '\0' || amount <= 0 || amount > INT_MAX) {
            fprintf(stderr, "Error: Please enter a valid positive integer for sleep duration.\n");
            return 1;
        }

        // Ensure the amount is positive
        if (amount > 0) {
            printf("Sleeping for %ld seconds...\n", amount);
            sleep(amount);
            printf("Awake after %ld seconds.\n", amount);
        } else {
            fprintf(stderr, "Error: Please enter a positive integer for sleep duration.\n");
            return 1;
        }
    } else {
        fprintf(stderr, "Usage: %s <seconds>\n", argv[0]);
        return 1;
    }

    return 0;
}
