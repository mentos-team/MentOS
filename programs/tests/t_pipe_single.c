/// @file t_pipe_single.c
/// @brief Test pipe system within a single process.
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
#include <strerror.h>

int main(int argc, char *argv[])
{
    int fds[2];

    const char write_msg[] =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n"
        "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.\n"
        "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris\n"
        "nisi ut aliquip ex ea commodo consequat.";
    char read_msg[sizeof(write_msg)];

    pr_info("\n\nStarting single-process pipe test.\n");

    // Create a pipe.
    if (pipe(fds) == -1) {
        pr_err("Failed to create pipe.\n");
        return 1;
    }
    pr_info("Pipe created: fds[0] = %d (read), fds[1] = %d (write).\n", fds[0], fds[1]);

    // Write a message to the pipe.
    if (write(fds[1], write_msg, strlen(write_msg)) == -1) {
        pr_err("Write to pipe failed.\n");
        close(fds[1]);
        close(fds[0]);
        return 1;
    }
    pr_info("Successfully wrote to pipe:"
            "\n----------------------------------------\n"
            "%s"
            "\n----------------------------------------\n",
            write_msg);

    // Close the write end of the pipe to simulate end of transmission.
    if (close(fds[1]) == -1) {
        pr_err("Failed to close the write end of the pipe: %s\n", strerror(errno));
    } else {
        pr_debug("Closed the write end of the pipe.\n");
    }

    // Read the message from the pipe.
    ssize_t bytes_read = read(fds[0], read_msg, sizeof(read_msg) - 1);
    if (bytes_read == -1) {
        pr_err("Read from pipe failed.\n");
        close(fds[0]);
        return 1;
    }

    // Null-terminate the read message and print it.
    read_msg[bytes_read] = '\0';
    pr_info("Successfully read from pipe"
            "\n----------------------------------------\n"
            "%s"
            "\n----------------------------------------\n",
            read_msg);

    // Close the read end of the pipe.
    if (close(fds[0]) == -1) {
        pr_err("Failed to close the read end of the pipe: %s\n", strerror(errno));
    } else {
        pr_debug("Closed the read end of the pipe.\n");
    }

    pr_info("Single-process pipe test completed.\n\n");
    return 0;
}
