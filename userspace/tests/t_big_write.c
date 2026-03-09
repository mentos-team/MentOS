/// @file t_big_write.c
/// @brief Test writing a big file.
/// @details This program tests writing a large amount of data to a file by
/// repeatedly writing a buffer filled with characters.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#define FILENAME    "/tmp/test.txt"
#define ITERATIONS  4
#define BUFFER_SIZE BUFSIZ

int write_test_data(const char *filename, int iterations)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to open file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }
    int result = EXIT_SUCCESS;
    char buffer[BUFFER_SIZE];
    for (unsigned times = 0; times < iterations; ++times) {
        for (unsigned i = 'A'; i < 'z'; ++i) {
            memset(buffer, i, sizeof(buffer));
            if (write(fd, buffer, sizeof(buffer)) < 0) {
                syslog(LOG_ERR, "Writing to file %s failed: %s\n", filename, strerror(errno));
                result = EXIT_FAILURE;
                goto write_close_and_cleanup;
            }
        }
    }

write_close_and_cleanup:
    // Close the file descriptor and unlink the file after the test is done.
    if (close(fd) < 0) {
        syslog(LOG_ERR, "Failed to close file %s: %s\n", filename, strerror(errno));
        result = EXIT_FAILURE;
    }
    // Unlink the file after the test is done.
    if (unlink(FILENAME) < 0) {
        syslog(LOG_ERR, "Failed to delete file %s: %s\n", FILENAME, strerror(errno));
        result = EXIT_FAILURE;
    }
    return result;
}

int read_and_verify_test_data(const char *filename, int iterations)
{
    int fd = open(filename, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to open file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }

    int result = EXIT_SUCCESS;
    char read_buf[BUFFER_SIZE];
    char expected[BUFFER_SIZE];

    for (unsigned pass = 0; pass < iterations; ++pass) {
        for (unsigned ch = 'A'; ch < 'z'; ++ch) {
            /* prepare the expected pattern for this character */
            memset(expected, ch, sizeof(expected));

            ssize_t got = read(fd, read_buf, sizeof(read_buf));
            if (got < 0) {
                syslog(LOG_ERR, "Reading from file %s failed: %s\n", filename, strerror(errno));
                result = EXIT_FAILURE;
                goto read_close_and_cleanup;
            }

            if (got != (ssize_t)sizeof(read_buf)) {
                syslog(LOG_ERR, "Unexpected read length %zd from %s\n", got, filename);
                result = EXIT_FAILURE;
                goto read_close_and_cleanup;
            }

            if (memcmp(read_buf, expected, sizeof(read_buf)) != 0) {
                syslog(LOG_ERR, "Data mismatch in file %s at iteration %u, char %c\n", filename, pass, ch);
                result = EXIT_FAILURE;
                goto read_close_and_cleanup;
            }
        }
    }

read_close_and_cleanup:
    // Close the file descriptor and unlink the file after the test is done.
    if (close(fd) < 0) {
        syslog(LOG_ERR, "Failed to close file %s: %s\n", filename, strerror(errno));
        result = EXIT_FAILURE;
    }
    // Unlink the file after the test is done.
    if (unlink(FILENAME) < 0) {
        syslog(LOG_ERR, "Failed to delete file %s: %s\n", FILENAME, strerror(errno));
        result = EXIT_FAILURE;
    }
    return result;
}

int main(int argc, char *argv[])
{
    if (write_test_data(FILENAME, ITERATIONS) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    if (read_and_verify_test_data(FILENAME, ITERATIONS) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
