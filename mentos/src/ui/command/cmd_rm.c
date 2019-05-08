///                MentOS, The Mentoring Operating system project
/// @file cmd_rm.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "strerror.h"

void cmd_rm(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("%s: missing operand.\n", argv[0]);
        printf("Try '%s --help' for more information.\n\n", argv[0]);

        return;
    }

    if (strcmp(argv[1], "--help") == 0)
    {
        printf("Remove (unlink) the FILE(s).\n");
        printf("Usage:\n");
        printf("    rm <filename>\n");

        return;
    }

    int fd = open(argv[1], O_RDONLY, 0);
    if (fd < 0)
    {
        printf("%s: cannot remove '%s': %s\n\n",
               argv[0], argv[1], "unknown"/*strerror(errno)*/);

        return;
    }

    close(fd);
    if (remove(argv[1]) != 0)
    {
        printf("rm: cannot remove '%s': Failed to remove file\n\n",
               argv[1]);

        return;
    }
    printf("\n");
}
