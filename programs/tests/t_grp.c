/// @file t_pwd.c
/// @brief Test the libc grp.h interface
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <grp.h>
#include <err.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strerror.h>

static void __test_getgrnam(void) {
    // Test that getpwnam matches the whole group name literally
    if (getgrnam("r") != NULL)
        errx(EXIT_FAILURE, "Group entry for non-existent group \"r\" found");

    if (getgrnam("root") == NULL)
        errx(EXIT_FAILURE, "Group entry for root group not found");
}

static void __test_getgrgid(void) {
    if (getgrgid(1337) != NULL)
        errx(EXIT_FAILURE, "Group entry for non-existent gid 1337 found");

    int gids[] = {0, 1000};
    for (int i = 0; i < sizeof(gids) / sizeof(int); i++) {
        if (getgrgid(gids[i]) == NULL)
            errx(EXIT_FAILURE, "Group entry for gid %d not found", gids[i]);
    }
}

int main(int argc, char *argv[])
{
    __test_getgrnam();
    __test_getgrgid();
}
