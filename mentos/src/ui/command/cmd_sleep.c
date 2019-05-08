///                MentOS, The Mentoring Operating system project
/// @file   sleep.c
/// @brief  
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "string.h"
#include "stdio.h"
#include "timer.h"
#include "clock.h"

void cmd_sleep(int argc, char ** argv)
{
    if (argc != 2)
    {
        printf("%s: missing operand.\n", argv[0]);
        printf("Try '%s --help' for more information.\n\n", argv[0]);

        return;
    }

    if (!strcmp(argv[1], "--help"))
    {
        printf("Usage: %s <seconds>\n\n", argv[0]);

        return;
    }

    int seconds = atoi(argv[1]);
    if (seconds <= 0)
    {
        printf("Error: You must provide a positive value (%d). \n\n", seconds);

        return;
    }

    time_t t0 = get_hour() * 60 * 60 + get_minute() * 60 + get_second();
    printf("Start sleeping at '%d' for %ds...\n", t0, seconds);
    sleep((time_t) seconds);
    time_t t1 = get_hour() * 60 * 60 + get_minute() * 60 + get_second();
    printf("End sleeping at '%d' after %ds.\n", t1, t1 - t0);
}
