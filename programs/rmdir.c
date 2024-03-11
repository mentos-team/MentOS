/// @file rmdir.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <strerror.h>

int main(int argc, char *argv[])
{
    // Check the number of arguments.
    if (argc != 2) {
        printf("Bad usage.\n");
        printf("Try 'rmdir --help' for more information.\n");
        return 1;
    }
    if (strcmp(argv[1], "--help") == 0) {
        printf("Removes a directory.\n");
        printf("Usage:\n");
        printf("    rmdir <directory>\n");
        return 0;
    }
    if (rmdir(argv[1]) == -1) {
        printf("%s: failed to remove '%s': %s\n", argv[0], argv[1], strerror(errno));
        return 1;
    }
    return 0;
}
