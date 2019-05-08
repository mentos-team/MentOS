///                MentOS, The Mentoring Operating system project
/// @file cmd_showpid.c
/// @brief  
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "stdio.h"
#include "unistd.h"
#include "scheduler.h"

void cmd_showpid(int argc, char **argv)
{
    printf("pid %d\n", getpid());
}
