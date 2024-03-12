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
#include <math.h>
#include <io/debug.h>

int create_file(const char *filename, mode_t mode)
{
    int fd = creat(filename, mode);
    if (fd < 0) {
        printf("Failed to create file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }
    close(fd);
    return EXIT_SUCCESS;
}

int check_content(const char *filename, const char *content, int length)
{
    pr_notice("Check content(%s, %s, %d)\n", filename, content, length);

    char buffer[256];
    memset(buffer, 0, 256);
    // Open the file.
    int fd = open(filename, O_RDONLY, 0);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }
    // Read the content.
    if (read(fd, &buffer, max(min(length, 256), 0)) < 0) {
        printf("Reading from file %s failed: %s\n", filename, strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    if (strcmp(buffer, content) != 0) {
        printf("Unexpected file content `%s`, expecting `%s`.\n", buffer, content);
        close(fd);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int write_content(const char *filename, const char *content, int length, int truncate, int append)
{
    pr_notice("Write content(%s, %s, %d, TRUNC: %d, APP: %d)\n", filename, content, length, truncate, append);

    int flags = O_WRONLY | (truncate ? O_TRUNC : append ? O_APPEND :
                                                          0);
    // Open the file.
    int fd = open(filename, flags, 0);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }
    // Read the content.
    if (write(fd, content, length) < 0) {
        printf("Writing on file %s failed: %s\n", filename, strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    close(fd);
    return EXIT_SUCCESS;
}

int test_write_read(const char *filename)
{
    char buf[7] = { 0 };
    // Create the file.
    if (create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) {
        return EXIT_FAILURE;
    }
    // Open the file.
    int fd = open(filename, O_WRONLY, 0);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }
    if (write(fd, "foo", 3) != 3) {
        printf("First write to %s failed: %s\n", filename, strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    if (write(fd, "bar", 3) != 3) {
        printf("Second write to %s failed: %s\n", filename, strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    // Close the file.
    close(fd);
    // Check the content.
    if (check_content(filename, "foobar", 6)) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int test_truncate(const char *filename)
{
    if (create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) {
        return EXIT_FAILURE;
    }
    if (write_content(filename, "foobar", 6, 0, 0)) {
        return EXIT_FAILURE;
    }
    if (check_content(filename, "foobar", 6)) {
        return EXIT_FAILURE;
    }
    if (write_content(filename, "bark", 4, 0, 0)) {
        return EXIT_FAILURE;
    }
    if (check_content(filename, "barkar", 6)) {
        return EXIT_FAILURE;
    }
    if (write_content(filename, "barf", 4, 1, 0)) {
        return EXIT_FAILURE;
    }
    if (check_content(filename, "barf", 6)) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int test_append(const char *filename)
{
    if (create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) {
        return EXIT_FAILURE;
    }
    if (write_content(filename, "fusro", 5, 0, 0)) {
        return EXIT_FAILURE;
    }
    if (write_content(filename, "dah", 3, 0, 1)) {
        return EXIT_FAILURE;
    }
    if (check_content(filename, "fusrodah", 8)) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    char *filename = "/home/user/test.txt";

    printf("Running `test_write_read`...\n");
    if (test_write_read(filename)) {
        unlink(filename);
        return EXIT_FAILURE;
    }
    unlink(filename);

    printf("Running `test_truncate`...\n");
    if (test_truncate(filename)) {
        unlink(filename);
        return EXIT_FAILURE;
    }
    unlink(filename);

    printf("Running `test_append`...\n");
    if (test_append(filename)) {
        unlink(filename);
        return EXIT_FAILURE;
    }
    unlink(filename);

    return EXIT_SUCCESS;
}
