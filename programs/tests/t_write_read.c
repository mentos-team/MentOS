/// @file t_write_read.c
/// @brief Test consecutive writes
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

int test_write_read()
{
    char *filename  = "t_write_read_file";
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

    int fd = creat(filename, mode);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", filename, strerror(errno));
        return 1;
    }

    if (write(fd, "foo", 3) != 3) {
        printf("First write to %s failed: %s\n", filename, strerror(errno));
        return 1;
    }

    if (write(fd, "bar", 3) != 3) {
        printf("Second write to %s failed: %s\n", filename, strerror(errno));
        return 1;
    }

    close(fd);

    fd = open(filename, O_RDONLY, 0);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", filename, strerror(errno));
        return 1;
    }

    ssize_t bytes_read;
    char buf[7];
    buf[6] = 0;
    if ((bytes_read = read(fd, &buf, 6)) < 0) {
        printf("Reading from file %s failed: %s\n", filename, strerror(errno));
        return 1;
    }
    close(fd);

    if (strcmp(buf, "foobar") != 0) {
        printf("Unexpected file content: %s\n", buf);
        return 1;
    }

    unlink(filename);
    return 0;
}

int test_truncate()
{
    char *filename = "test.txt";
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    char buffer[7];
    ssize_t bytes_read;
    int fd;

    // Create the file.
    fd = creat(filename, mode);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", filename, strerror(errno));
        return 1;
    }
    if (write(fd, "foobar", 6) != 6) {
        printf("First write to %s failed: %s\n", filename, strerror(errno));
        return 1;
    }
    close(fd);

    // Override the content.
    fd = open(filename, O_RDWR | O_TRUNC, 0);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", filename, strerror(errno));
        return 1;
    }
    if (write(fd, "bar", 3) != 3) {
        printf("Overriding the content of %s failed: %s\n", filename, strerror(errno));
        return 1;
    }
    // Go back to the beginning.
    lseek(fd, 0, SEEK_SET);
    // Read the content.
    if ((bytes_read = read(fd, &buffer, 6)) < 0) {
        printf("Reading from file %s failed: %s\n", filename, strerror(errno));
        return 1;
    }
    // Close the file.
    close(fd);

    printf("We found `%s`\n", buffer);

    if (strcmp(buffer, "bar") != 0) {
        printf("Unexpected file content: %s\n", buffer);
        return 1;
    }

    // Delete the file.
    unlink(filename);
    return 0;
}

int main(int argc, char *argv[])
{
    printf("Running `test_write_read`...\n");
    if (test_write_read()) {
        exit(EXIT_FAILURE);
    }
    printf("Running `test_truncate`...\n");
    if (test_truncate()) {
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
