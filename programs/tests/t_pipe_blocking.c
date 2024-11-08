/// @file t_pipe_blocking.c
/// @brief Test blocking pipe operations between parent and child process.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// ============================================================================
// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TEST  ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.
// ============================================================================

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

int main()
{
    int fds[2];
    char write_msg[]                 = "Blocking test message";
    char read_msg[sizeof(write_msg)] = { 0 };
    ssize_t bytes_written, bytes_read;
    int error_code = 0;

    // Create a pipe.
    if (pipe(fds) == -1) {
        pr_err("Failed to create pipe\n");
        return 1;
    }

    // Fork a child process.
    pid_t pid = fork();

    if (pid == -1) {
        pr_err("Failed to fork process\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    } else if (pid == 0) {
        // Child process: reads from the pipe.
        close(fds[1]); // Close unused write end

        pr_info("Child waiting to read from pipe...\n");
        do {
            bytes_read = read(fds[0], read_msg, sizeof(read_msg));
            if (bytes_read > 0) {
                pr_info("Child read message: '%s' (%ld bytes)\n", read_msg, bytes_read);
            } else if ((bytes_read == -1) && (errno != EAGAIN)) {
                pr_err("Error occurred during read in child process\n");
                error_code = 1;
                break;
            }
        } while (bytes_read != 0);

        close(fds[0]); // Close read end
        return error_code;

    } else {
        // Parent process: writes to the pipe.
        close(fds[0]); // Close unused read end.

        sleep(2);

        pr_info("Parent writing to pipe...\n");
        bytes_written = write(fds[1], write_msg, sizeof(write_msg));

        if (bytes_written > 0) {
            pr_info("Parent wrote message: '%s' (%ld bytes)\n", write_msg, bytes_written);
        } else if (bytes_written == -1) {
            pr_err("Error occurred during write in parent process\n");
            error_code = 1;
        }

        sleep(1);

        close(fds[1]); // Close write end.
        wait(NULL);    // Wait for child to finish
        return error_code;
    }
}
