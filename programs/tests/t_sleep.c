/// @file t_sleep.c
/// @brief Demonstrates the use of the sleep function to pause program execution
/// for a specified duration.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    // Sleep for 500 ms.
    struct timespec req = {0, 500000000}; // 500 ms = 500,000,000 nanoseconds

    if (nanosleep(&req, NULL) != 0) {
        fprintf(stderr, "nanosleep error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
