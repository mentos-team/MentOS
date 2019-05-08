///                MentOS, The Mentoring Operating system project
/// @file cmd_clear.c
/// @brief  
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "video.h"

void cmd_clear(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    video_clear();
}
