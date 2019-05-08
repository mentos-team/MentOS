///                MentOS, The Mentoring Operating system project
/// @file cmd_more.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "strerror.h"

void cmd_more(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("%s: missing operand.\n", argv[0]);
        printf("Try '%s --help' for more information.\n\n", argv[0]);

        return;
    }

    if (strcmp(argv[1], "--help") == 0)
    {
        printf("Prints the content of the given file.\n");
        printf("Usage:\n");
        printf("    %s <file>\n\n", argv[0]);

        return;
    }

    int fd = open(argv[1], O_RDONLY, 42);
    if (fd < 0)
    {
        printf("%s: Cannot stat file '%s': %s\n\n",
               argv[0], argv[1],"unknown"/*strerror(errno)*/);

        return;
    }

    char c;
    // Put on the standard output the characters.
    while (read(fd, &c, 1) > 0)
    {
        putchar(c);
    }
    putchar('\n');
    putchar('\n');
    close(fd);
}
