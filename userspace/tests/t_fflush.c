/// @file t_fflush.c
/// @brief Test program for the fflush function.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    // Test 1: fflush with stdout
    syslog(LOG_INFO, "[t_fflush] Testing fflush with stdout...");
    if (fflush(STDOUT_FILENO) == 0) {
        syslog(LOG_INFO, "[t_fflush]  SUCCESS\n");
    } else {
        syslog(LOG_INFO, "[t_fflush]  FAILED\n");
        return 1;
    }

    // Test 2: fflush with stderr
    syslog(LOG_INFO, "[t_fflush] Testing fflush with stderr...");
    if (fflush(STDERR_FILENO) == 0) {
        syslog(LOG_INFO, "[t_fflush]  SUCCESS\n");
    } else {
        syslog(LOG_INFO, "[t_fflush]  FAILED\n");
        return 1;
    }

    // Test 3: fflush with negative value (flush all streams)
    syslog(LOG_INFO, "[t_fflush] Testing fflush with -1 (all streams)...");
    if (fflush(-1) == 0) {
        syslog(LOG_INFO, "[t_fflush]  SUCCESS\n");
    } else {
        syslog(LOG_INFO, "[t_fflush]  FAILED\n");
        return 1;
    }

    // Test 4: fflush with a file descriptor
    // Try to create a file in /tmp (which is now created by FHS initialization)
    int fd = open("/tmp/fflush_test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        // If /tmp doesn't work, try /home
        fd = open("/home/fflush_test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
    if (fd >= 0) {
        write(fd, "test data", 9);
        syslog(LOG_INFO, "[t_fflush] Testing fflush with file descriptor...");
        if (fflush(fd) == 0) {
            syslog(LOG_INFO, "[t_fflush]  SUCCESS\n");
        } else {
            syslog(LOG_INFO, "[t_fflush]  FAILED\n");
            close(fd);
            return 1;
        }
        close(fd);
    } else {
        // If we can't create files in either location, that's okay for this test
        syslog(LOG_INFO, "[t_fflush] Skipping file descriptor test (no writable directory found).\n");
    }

    syslog(LOG_INFO, "[t_fflush] All fflush tests passed!\n");
    return 0;
}
