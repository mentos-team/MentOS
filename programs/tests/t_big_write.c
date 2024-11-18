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

#define FILENAME    "/home/user/test.txt"
#define ITERATIONS  8
#define BUFFER_SIZE BUFSIZ

int main(int argc, char *argv[])
{
    mode_t mode                    = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    char write_buffer[BUFFER_SIZE] = { 0 };
    char read_buffer[BUFFER_SIZE]  = { 0 };

    // Open the file with specified flags and mode.
    int fd = open(FILENAME, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) {
        fprintf(stderr, "Failed to open file %s: %s\n", FILENAME, strerror(errno));
        return EXIT_FAILURE;
    }

    // Write test data to the file.
    for (unsigned times = 0; times < ITERATIONS; ++times) {
        for (unsigned i = 'A'; i < 'z'; ++i) {
            memset(write_buffer, i, sizeof(write_buffer));
            if (write(fd, write_buffer, sizeof(write_buffer)) < 0) {
                fprintf(stderr, "Writing to file %s failed: %s\n", FILENAME, strerror(errno));
                close(fd);
                unlink(FILENAME);
                return EXIT_FAILURE;
            }
        }
    }

    // Close the file descriptor.
    if (close(fd) < 0) {
        fprintf(stderr, "Failed to close file %s: %s\n", FILENAME, strerror(errno));
        unlink(FILENAME);
        return EXIT_FAILURE;
    }

    // Open the file with specified flags and mode.
    fd = open(FILENAME, O_RDONLY, mode);
    if (fd < 0) {
        fprintf(stderr, "Failed to open file %s: %s\n", FILENAME, strerror(errno));
        unlink(FILENAME);
        return EXIT_FAILURE;
    }

    // Read and verify data from the file.
    for (unsigned times = 0; times < ITERATIONS; ++times) {
        for (unsigned i = 'A'; i < 'z'; ++i) {
            memset(write_buffer, i, sizeof(write_buffer));
            if (read(fd, read_buffer, sizeof(read_buffer)) < 0) {
                fprintf(stderr, "Reading from file %s failed: %s\n", FILENAME, strerror(errno));
                close(fd);
                unlink(FILENAME);
                return EXIT_FAILURE;
            }

            // Verify read data matches what was written.
            if (memcmp(write_buffer, read_buffer, sizeof(write_buffer)) != 0) {
                fprintf(stderr, "Data mismatch in file %s at iteration %u, char %c\n", FILENAME, times, i);
                close(fd);
                unlink(FILENAME);
                return EXIT_FAILURE;
            }
        }
    }

    // Close the file descriptor.
    if (close(fd) < 0) {
        fprintf(stderr, "Failed to close file %s: %s\n", FILENAME, strerror(errno));
        unlink(FILENAME);
        return EXIT_FAILURE;
    }

    // Delete the test file.
    if (unlink(FILENAME) < 0) {
        fprintf(stderr, "Failed to delete file %s: %s\n", FILENAME, strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
