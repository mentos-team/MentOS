/// @file t_pwd.c
/// @brief Test the libc pwd.h interface
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <pwd.h>
#include <err.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strerror.h>

static void __test_getpwnam(void) {
    // Test that getpwnam matches the whole user name literally
    if (getpwnam("r") != NULL)
        errx(EXIT_FAILURE, "Password entry for non-existent user \"r\" found");

    if (getpwnam("root") == NULL)
        errx(EXIT_FAILURE, "Password entry for root user not found");
}

static void __test_getpwuid(void) {
    if (getpwuid(1337) != NULL)
        errx(EXIT_FAILURE, "Password entry for non-existent uid 1337 found");

    if (getpwuid(0) == NULL)
        errx(EXIT_FAILURE, "Password entry for uid 0 not found");
}

int main(int argc, char *argv[])
{
    __test_getpwnam();
    __test_getpwuid();
}
