/// @file t_chdir.c
/// @brief Test program for the chdir system call.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    const char *directory = "/home"; // Default directory if none is provided
    char cwd[1024];

    // Try to change the current working directory.
    if (chdir(directory) == 0) {
        // Success, print the current working directory.
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            // Compare cwd and the expected directory.
            if (strcmp(cwd, directory) == 0) {
                printf("Successfully changed to the directory.\n");
                return EXIT_SUCCESS;
            } else {
                printf("Directory change failed or directory differs: expected %s but got %s\n", directory, cwd);
            }
        } else {
            perror("getcwd failed");
        }
    } else {
        perror("chdir failed");
    }
    return EXIT_FAILURE;
}