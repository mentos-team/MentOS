///                MentOS, The Mentoring Operating system project
/// @file cmd_touch.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"

void cmd_touch(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("%s: missing operand.\n", argv[0]);
        printf("Try '%s --help' for more information.\n\n", argv[0]);

        return;
    }

    if (strcmp(argv[1], "--help") == 0)
    {
        printf("Updates modification times of a given fine. If the does not"
               "exists, it creates it.\n");
        printf("Usage:\n");
        printf("    touch <filename>\n");

        return;
    }

    int fd = open(argv[1], O_RDONLY, 0);
    if (fd < 0)
    {
        fd = open(argv[1], O_CREAT, 0);
        if (fd >= 0)
        {
            close(fd);
        }
    }
    printf("\n");
}
