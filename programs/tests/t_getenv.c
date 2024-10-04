/// @file t_getenv.c
/// @brief Test the getenv function.
/// @details This program tests the `getenv` function by retrieving the value of
/// an environment variable and printing it. If the environment variable is not
/// set, it prints an error message and exits with a failure status.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <sys/unistd.h>

int main(int argc, char *argv[])
{
    // Retrieve the value of the environment variable "ENV_VAR".
    char *env_var = getenv("ENV_VAR");
    if (env_var == NULL) {
        fprintf(STDERR_FILENO, "Failed to get env: `ENV_VAR`: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Print the value of the environment variable.
    printf("ENV_VAR = %s\n", env_var);
    return EXIT_SUCCESS;
}
