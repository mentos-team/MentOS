///                MentOS, The Mentoring Operating system project
/// @file   credits.c
/// @brief  
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "video.h"
#include "stdio.h"
#include "version.h"

void cmd_credits(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    video_set_color(BRIGHT_BLUE);
    printf(OS_NAME" Credits\n\n");
    printf("Main Developers:\n");
    video_set_color(GREEN);
    printf("Enrico Fraccaroli (Galfurian)\n");
    printf("Alessando Danese\n");
    video_set_color(BRIGHT_BLUE);
    printf("Developers:\n");
    video_set_color(GREEN);
    printf("Luigi C.\n"
               "Mirco D.\n\n");
    video_set_color(WHITE);
}
