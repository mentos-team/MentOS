/// @file t_pipe_eof_data.c
/// @brief Regression test for #430: drain buffered pipe data before EOF.
/// @details The last writer may close while bytes are still queued. Reads must
/// return those bytes before a later read reports EOF.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

int main(void)
{
    int fds[2];
    const char payload[] = "buffered pipe data";
    char received[sizeof(payload)] = {0};

    if (pipe(fds) < 0) {
        syslog(LOG_ERR, "[t_pipe_eof_data] pipe failed");
        return EXIT_FAILURE;
    }

    ssize_t written = write(fds[1], payload, sizeof(payload));
    if (written != (ssize_t)sizeof(payload)) {
        syslog(LOG_ERR, "[t_pipe_eof_data] write returned %d, expected %d", (int)written, (int)sizeof(payload));
        close(fds[0]);
        close(fds[1]);
        return EXIT_FAILURE;
    }

    if (close(fds[1]) < 0) {
        syslog(LOG_ERR, "[t_pipe_eof_data] closing last writer failed");
        close(fds[0]);
        return EXIT_FAILURE;
    }

    ssize_t bytes_read = read(fds[0], received, sizeof(received));
    if ((bytes_read != (ssize_t)sizeof(payload)) || (memcmp(received, payload, sizeof(payload)) != 0)) {
        syslog(
            LOG_ERR, "[t_pipe_eof_data] read returned %d bytes after writer close; expected queued payload (%d bytes)",
            (int)bytes_read, (int)sizeof(payload));
        close(fds[0]);
        return EXIT_FAILURE;
    }

    close(fds[0]);
    syslog(LOG_INFO, "[t_pipe_eof_data] drained queued data after the last writer closed");
    return EXIT_SUCCESS;
}
