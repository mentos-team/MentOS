///                MentOS, The Mentoring Operating system project
/// @file echo.c
/// @brief  
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "stdio.h"

void cmd_echo(int argc, char **argv)
{
    int i = argc;
    int j = 0;
    if (argc == 1)
    {
        printf("");
    }
    else
    {
        while (--i > 0)
        {
            printf("%s ", argv[++j]);
        }
    }
    printf("\n");
}
