///                MentOS, The Mentoring Operating system project
/// @file cmd_rmdir.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "strerror.h"

void cmd_rmdir(int argc, char **argv)
{
    // Check the number of arguments.
    if (argc != 2)
    {
        printf("Bad usage.\n");
        printf("Try 'rmdir --help' for more information.\n");

        return;
    }

    if (strcmp(argv[1], "--help") == 0)
    {
        printf("Removes a directory.\n");
        printf("Usage:\n");
        printf("    rmdir <directory>\n");

        return;
    }

    if (rmdir(argv[1]) != 0)
    {
        printf("%s: failed to remove '%s': %s\n\n",
               argv[0], argv[1], "unknown"/*strerror(errno)*/);

        return;
    }
}
