/// @file readline.c
/// @brief Implementation of a function that reads a line from a file descriptor.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[RDLINE]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "readline.h"

#include "stdio.h"
#include "string.h"
#include "sys/unistd.h"

int readline(int fd, char *buffer, size_t buflen, ssize_t *read_len)
{
    // Error check: Ensure the buffer is not NULL and has sufficient length.
    if (!buffer || buflen == 0) {
        pr_err("Invalid buffer or buffer length.\n");
        return 0; // Invalid input, cannot proceed.
    }

    ssize_t length, rollback, num_read;
    unsigned char found_newline = 1;

    // Initialize the buffer to ensure it starts empty.
    memset(buffer, 0, buflen);

    // Read from the file descriptor into the buffer.
    num_read = read(fd, buffer, buflen);

    // Error check: If read() fails, return -1 to indicate an error.
    if (num_read < 0) {
        pr_err("Failed to read from file descriptor.\n");
        return -1;
    }

    // If nothing was read, return 0 to indicate the end of the file.
    if (num_read == 0) {
        return 0;
    }

    // Search for newline or termination character.
    char *newline = strchr(buffer, '\n');
    if (newline == NULL) {
        found_newline = 0;
        // Search for EOF.
        newline = strchr(buffer, EOF);
        if (newline == NULL) {
            // Search for null terminator.
            newline = strchr(buffer, '\0');
            if (newline == NULL) {
                // No termination character found.
                return 0;
            }
        }
    }

    // Compute the length of the string up to the newline or termination
    // character.
    length = (newline - buffer);
    if (length <= 0) {
        return 0;
    }

    // Close the string by adding a null terminator.
    buffer[length] = '\0';

    // Compute how much we need to rollback the file position after reading.
    rollback = length - num_read + 1;
    if (rollback > 1) {
        return 0; // Rollback value seems invalid.
    }

    // Adjust the file's read position using lseek to undo the extra read
    // characters.
    lseek(fd, rollback, SEEK_CUR);

    // Set the number of bytes read if the caller provided a pointer.
    if (read_len) {
        *read_len = length;
    }

    // Return 1 if a newline was found, -1 otherwise (partial data read).
    return (found_newline) ? 1 : -1;
}
