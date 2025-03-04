/// @file uptime.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int uptime_fd = open("/proc/uptime", O_RDONLY, 0600);
    if (uptime_fd == -1) {
        printf("Impossible to retrieve /proc/uptime file");
        return -1;
    }

    // Read the content of /proc/uptime

    char buffer[64];
    if (read(uptime_fd, buffer, sizeof(char) * 64) == -1) {
        printf("Impossible to read /proc/uptime content");
        return -1;
    }

    char *endptr;
    long uptime = strtol(buffer, &endptr, 10);
    if (*endptr != '\0' && *endptr != ' ') {
        printf("Conversion error occurred\n");
        return -1;
    }

    // Transform uptime into hours, days, mins, and seconds

    int updays    = uptime / 86400;
    int uphours   = (uptime - (updays * 86400)) / 3600;
    int upmins    = (uptime - (updays * 86400) - (uphours * 3600)) / 60;
    int upseconds = (uptime - (updays * 86400) - (uphours * 3600) - (upmins * 60));

    printf("Days: %d Hours: %d Minutes: %d Seconds: %d \n", updays, uphours, upmins, upseconds);
    close(uptime_fd);
    return 0;
}
