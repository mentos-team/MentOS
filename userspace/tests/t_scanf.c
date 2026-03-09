/// @file t_scanf.c
/// @brief Test the scanf function.
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdio.h>
#include <syslog.h>

int main(void)
{
    int number;
    char name[100];

    syslog(LOG_INFO, "[t_scanf] Enter a number: ");
    if (scanf("%d", &number) != 1) {
        syslog(LOG_INFO, "[t_scanf] Failed to read number.\n");
        return 1;
    }

    syslog(LOG_INFO, "[t_scanf] Enter your name: ");
    if (scanf("%99s", name) != 1) { // %99s to prevent buffer overflow
        syslog(LOG_INFO, "[t_scanf] Failed to read name.\n");
        return 1;
    }

    syslog(LOG_INFO, "[t_scanf] Hello, %s! You entered %d.\n", name, number);
    return 0;
}
