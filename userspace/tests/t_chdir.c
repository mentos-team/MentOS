/// @file t_chdir.c
/// @brief Test program for the chdir system call.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

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
                syslog(LOG_INFO, "[t_chdir] Successfully changed to the directory.\n");
                return EXIT_SUCCESS;
            }
            syslog(LOG_INFO, "[t_chdir] Directory change failed or directory differs: expected %s but got %s\n", directory, cwd);

        } else {
            syslog(LOG_ERR, "[t_chdir] getcwd failed: %s", strerror(errno));
        }
    } else {
        syslog(LOG_ERR, "[t_chdir] chdir failed: %s", strerror(errno));
    }
    return EXIT_FAILURE;
}
