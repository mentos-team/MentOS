/// @file t_pipe_non_blocking.c
/// @brief Test non-blocking pipe operations within a single process.
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
#include <fcntl.h>
#include <string.h>
#include <errno.h>

int main()
{
    int fds[2];
    char write_msg[]                 = "Non-blocking test message";
    char read_msg[sizeof(write_msg)] = { 0 };
    ssize_t bytes_written, bytes_read;
    size_t write_attempts = 0, read_attempts = 0, total_written = 0, total_read = 0;
    int error_code = 0;

    // Create a pipe.
    if (pipe(fds) == -1) {
        pr_err("Failed to create pipe\n");
        return 1;
    }

    // Set both ends of the pipe to non-blocking mode.
    if (fcntl(fds[0], F_SETFL, O_NONBLOCK) == -1 || fcntl(fds[1], F_SETFL, O_NONBLOCK) == -1) {
        pr_err("Failed to set pipe to non-blocking mode\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    // Attempt to fill the pipe with non-blocking writes.
    pr_info("Testing non-blocking write on full pipe:\n");
    while ((bytes_written = write(fds[1], write_msg, sizeof(write_msg))) > 0) {
        write_attempts++;
        total_written += bytes_written;
    }

    // Check for EAGAIN, indicating the pipe is full as expected in non-blocking mode.
    if (bytes_written == -1 && errno == EAGAIN) {
        pr_info("Non-blocking write returned EAGAIN as expected after %u writes (%u bytes).\n", write_attempts, total_written);
    } else if (bytes_written == -1) {
        pr_err("Unexpected error occurred during write\n");
        error_code = 1;
    }

    // Drain the pipe by reading until empty.
    pr_info("Draining the pipe to test non-blocking read:\n");
    while ((bytes_read = read(fds[0], read_msg, sizeof(read_msg))) > 0) {
        read_attempts++;
        total_read += bytes_read;
    }

    // Check for EAGAIN, indicating the pipe is empty as expected in non-blocking mode.
    if (bytes_read == -1 && errno == EAGAIN) {
        pr_info("Non-blocking read returned EAGAIN as expected after %u reads (%u bytes).\n", read_attempts, total_read);
    } else if (bytes_read == -1) {
        pr_err("Unexpected error occurred during read\n");
        error_code = 1;
    }

    // Verify total bytes written and read match.
    if (total_written != total_read) {
        pr_err("Mismatch between total written (%u) and total read (%u) bytes.\n", total_written, total_read);
        error_code = 1;
    }

    // Close the pipe descriptors and return.
    close(fds[0]);
    close(fds[1]);

    return error_code;
}
