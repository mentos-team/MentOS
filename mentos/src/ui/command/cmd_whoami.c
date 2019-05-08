///                MentOS, The Mentoring Operating system project
/// @file cmd_whoami.c
/// @brief  
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "stdio.h"

void cmd_whoami(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    printf("%s\n", current_user.username);
}
