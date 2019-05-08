///                MentOS, The Mentoring Operating system project
/// @file cmd_pwd.c
/// @brief  
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <process/process.h>
#include <libc/stdlib.h>
#include "commands.h"
#include "stdio.h"

void cmd_pwd(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    char cwd[MAX_PATH_LENGTH];
    getcwd(cwd, MAX_PATH_LENGTH);
    printf("%s\n", cwd);
}
