/// @file mkdir.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <strerror.h>

int main(int argc, char *argv[])
{
    // Check the number of arguments.
    if (argc != 2) {
        printf("%s: missing operand.\n", argv[0]);
        printf("Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "--help") == 0) {
        printf("Creates a new directory.\n");
        printf("Usage:\n");
        printf("    %s <directory>\n", argv[0]);
        return 0;
    }
    if (mkdir(argv[1], S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) {
        printf("mkdir: cannot create directory '%s': %s\n", argv[1], strerror(errno));
    }
    putchar('\n');
    return 0;
}
