/// @file t_pwd.c
/// @brief Test the libc pwd.h interface for user and group management.
/// @details This program tests the password database interface provided by
/// libc's pwd.h. It checks for valid and invalid user names and UIDs, ensuring
/// proper functionality of `getpwnam` and `getpwuid`.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <unistd.h>

/// @brief Test `getpwnam` for valid and invalid user names.
/// @details This function checks if `getpwnam` correctly handles non-existent users
/// and retrieves the correct information for an existing user like "root".
static void __test_getpwnam(void)
{
    // Check for a non-existent user
    if (getpwnam("r") != NULL) {
        // If "r" is found, which is unexpected, exit with an error
        errx(EXIT_FAILURE, "Password entry for non-existent user \"r\" found");
    }

    // Check for the root user, which should always exist
    if (getpwnam("root") == NULL) {
        // If "root" is not found, exit with an error
        errx(EXIT_FAILURE, "Password entry for root user not found");
    } else {
        // If the root entry is found, print confirmation for debugging purposes
        printf("Password entry for root user found.\n");
    }
}

/// @brief Test `getpwuid` for valid and invalid user IDs.
/// @details This function checks if `getpwuid` correctly handles non-existent UIDs
/// and retrieves the correct information for a valid UID like 0 (root).
static void __test_getpwuid(void)
{
    // Check for a non-existent UID
    if (getpwuid(1337) != NULL) {
        // If UID 1337 is found, which is unexpected, exit with an error
        errx(EXIT_FAILURE, "Password entry for non-existent UID 1337 found");
    }

    // Check for the root UID, which should always exist
    if (getpwuid(0) == NULL) {
        // If UID 0 is not found, exit with an error
        errx(EXIT_FAILURE, "Password entry for UID 0 (root) not found");
    } else {
        // If the UID 0 entry is found, print confirmation for debugging purposes
        printf("Password entry for UID 0 (root) found.\n");
    }
}

/// @brief Main function that runs the tests for `getpwnam` and `getpwuid`.
/// @return Returns EXIT_SUCCESS if all tests pass, otherwise exits with failure.
int main(int argc, char *argv[])
{
    // Run the test for `getpwnam` function
    __test_getpwnam();

    // Run the test for `getpwuid` function
    __test_getpwuid();

    // If both tests pass, return success
    return EXIT_SUCCESS;
}
