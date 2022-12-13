/// @file t_sleep.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

int main(int argc, char *argv[])
{
    printf("Sleeping for 4 seconds... ");
    sleep(4);
    printf("COMPLETED.\n");

    return 0;
}
