///                MentOS, The Mentoring Operating system project
/// @file cmd_newfile.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "stdio.h"
#include "fcntl.h"
#include "string.h"
#include "unistd.h"
#include "strerror.h"
#include <misc/debug.h>

void cmd_newfile(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("%s: missing operand.\n", argv[0]);
        printf("Try '%s --help' for more information.\n\n", argv[0]);

        return;
    }

    if (strcmp(argv[1], "--help") == 0)
    {
        printf("Makes a new file, and prompt for it's content.\n");
        printf("Usage:\n");
        printf("    %s <filename>\n", argv[0]);

        return;
    }

    char text[256];
    printf("Filename: %s\n", argv[1]);
    int fd = open(argv[1], O_RDWR | O_CREAT | O_APPEND, 0);
    if (fd < 0)
    {
        printf("%s: Cannot create file '%s': %s\n\n",
               argv[0], argv[1], "unknown"/*strerror(errno)*/);

        return;
    }

    printf("Type one line of text here (new line to complete):\n");
    scanf("%s", text);
    if (write(fd, text, strlen(text)) == -1)
    {
        printf("%s: Cannot write on file '%s': %s\n\n",
               argv[0], argv[1], "unknown"/*strerror(errno)*/);

        return;
    }

    if (close(fd) == -1)
    {
        printf("%s: Cannot close file '%s': %s\n\n",
               argv[0], argv[1], "unknown"/*strerror(errno)*/);

        return;
    }
}
