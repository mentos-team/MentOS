/// @file t_big_write.c
/// @brief Test writing a big file.
/// @details This program tests writing a large amount of data to a file by
/// repeatedly writing a buffer filled with characters.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strerror.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int flags               = O_WRONLY | O_CREAT | O_TRUNC;
    mode_t mode             = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    char *filename          = "/home/user/test.txt";
    char buffer[4 * BUFSIZ] = { 0 };

    // Open the file with specified flags and mode.
    int fd = open(filename, flags, mode);
    if (fd < 0) {
        // Error handling for file open failure.
        fprintf(STDERR_FILENO, "Failed to open file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }

    // Write the content to the file.
    for (unsigned times = 0; times < 128; ++times) {
        for (unsigned i = 'A'; i < 'z'; ++i) {
            // Fill the buffer with the character 'i'.
            memset(buffer, i, 4 * BUFSIZ);
            // Write the buffer to the file.
            if (write(fd, buffer, 4 * BUFSIZ) < 0) {
                // Error handling for write failure.
                fprintf(STDERR_FILENO, "Writing to file %s failed: %s\n", filename, strerror(errno));
                // Close the file descriptor.
                if (close(fd) < 0) {
                    fprintf(STDERR_FILENO, "Failed to close file %s: %s\n", filename, strerror(errno));
                }
                // Delete the file.
                if (unlink(filename) < 0) {
                    fprintf(STDERR_FILENO, "Failed to delete file %s: %s\n", filename, strerror(errno));
                }
                return EXIT_FAILURE;
            }
        }
    }

    // Close the file descriptor.
    if (close(fd) < 0) {
        fprintf(STDERR_FILENO, "Failed to close file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }
    // Delete the file.
    if (unlink(filename) < 0) {
        fprintf(STDERR_FILENO, "Failed to delete file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
