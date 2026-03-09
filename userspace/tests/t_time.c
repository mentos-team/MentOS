/// @file t_time.c
/// @brief Test program for the time() function.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <syslog.h>
#include <time.h>

int main(void)
{
    // Get the current time
    time_t current_time = time(NULL);

    // Check if the time function failed
    if (current_time == (time_t)-1) {
        syslog(LOG_ERR, "Error: time() failed: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    // Convert to local time and print the result
    char *time_str = ctime(&current_time);
    if (time_str == NULL) {
        syslog(LOG_ERR, "Error: ctime() failed: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "Current time is: `%s`\n", time_str);

    return EXIT_SUCCESS;
}
