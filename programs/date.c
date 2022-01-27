/// @file date.c
/// @brief `date` program.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <time.h>

int main(int argc, char** argv)
{
    time_t rawtime = time(NULL);
    printf("Seconds since 01/01/1970 : %u\n", rawtime);
    tm_t* timeinfo = localtime(&rawtime);
    printf(
        "It's %2d:%2d:%2d, %d weekday, %02d/%02d/%4d\n",
        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
        timeinfo->tm_wday,
        timeinfo->tm_mday, timeinfo->tm_mon, timeinfo->tm_year
    );
    return 0;
}
