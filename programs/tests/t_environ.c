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
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    const char *env_var       = "TEST_ENV";
    const char *initial_value = "InitialValue";
    const char *updated_value = "UpdatedValue";

    // Set the environment variable
    if (setenv(env_var, initial_value, 1) != 0) {
        perror("setenv failed");
        return EXIT_FAILURE;
    }

    // Retrieve the environment variable
    const char *value = getenv(env_var);
    if (!value) {
        fprintf(stderr, "getenv failed: Environment variable %s not found.\n", env_var);
        return EXIT_FAILURE;
    }

    // Verify the retrieved value matches the set value
    if (strcmp(value, initial_value) != 0) {
        fprintf(stderr, "Mismatch: Expected '%s', but got '%s'.\n", initial_value, value);
        return EXIT_FAILURE;
    }

    // Update the environment variable
    if (setenv(env_var, updated_value, 1) != 0) {
        perror("setenv failed (update)");
        return EXIT_FAILURE;
    }

    // Retrieve the updated environment variable
    value = getenv(env_var);
    if (!value) {
        fprintf(stderr, "getenv failed: Environment variable %s not found after update.\n", env_var);
        return EXIT_FAILURE;
    }

    // Verify the retrieved value matches the updated value
    if (strcmp(value, updated_value) != 0) {
        fprintf(stderr, "Mismatch after update: Expected '%s', but got '%s'.\n", updated_value, value);
        return EXIT_FAILURE;
    }

    // Print success message
    printf("Environment variable %s tested successfully.\n", env_var);

    return EXIT_SUCCESS;
}
