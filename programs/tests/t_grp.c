/// @file t_pwd.c
/// @brief Test the libc grp.h interface.
/// @details This program tests the `getgrnam` and `getgrgid` functions from the
/// libc `grp.h` interface. It verifies that the functions correctly handle both
/// existing and non-existent group names and GIDs. If any test fails, the
/// program exits with an error message and a failure status.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <grp.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strerror.h>
#include <sys/unistd.h>

/// @brief Test the getgrnam function.
/// @details This function tests that `getgrnam` correctly handles both existing
/// and non-existent group names.
static void __test_getgrnam(void)
{
    // Test that getgrnam returns NULL for a non-existent group name.
    if (getgrnam("r") != NULL) {
        errx(EXIT_FAILURE, "Group entry for non-existent group \"r\" found");
    }

    // Test that getgrnam returns a valid entry for the "root" group.
    if (getgrnam("root") == NULL) {
        errx(EXIT_FAILURE, "Group entry for root group not found");
    }
}

/// @brief Test the getgrgid function.
/// @details This function tests that `getgrgid` correctly handles both existing
/// and non-existent GIDs.
static void __test_getgrgid(void)
{
    // Test that getgrgid returns NULL for a non-existent GID.
    if (getgrgid(1337) != NULL) {
        errx(EXIT_FAILURE, "Group entry for non-existent gid 1337 found");
    }

    // Test that getgrgid returns a valid entry for existing GIDs.
    int gids[] = { 0, 1000 };
    for (int i = 0; i < sizeof(gids) / sizeof(int); i++) {
        if (getgrgid(gids[i]) == NULL) {
            errx(EXIT_FAILURE, "Group entry for gid %d not found", gids[i]);
        }
    }
}

int main(int argc, char *argv[])
{
    // Test the getgrnam function.
    __test_getgrnam();
    // Test the getgrgid function.
    __test_getgrgid();

    return EXIT_SUCCESS;
}
