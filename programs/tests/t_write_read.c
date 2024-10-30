/// @file t_write_read.c
/// @brief Test consecutive writes and file operations.
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

/// @brief Creates a file with the specified name and mode.
/// @param filename The name of the file to create.
/// @param mode The permissions for the file.
/// @return EXIT_SUCCESS on success, or EXIT_FAILURE on failure.
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

/// @brief Checks the content of a file against the expected content.
/// @param filename The name of the file to check.
/// @param content The expected content of the file.
/// @param length The length of the expected content.
/// @return EXIT_SUCCESS on matching content, or EXIT_FAILURE on failure.
int check_content(const char *filename, const char *content, int length)
{
    char buffer[256];
    // Clear the buffer.
    memset(buffer, 0, 256);

    // Open the file for reading.
    int fd = open(filename, O_RDONLY, 0);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }

    // Read the content from the file.
    if (read(fd, &buffer, max(min(length, 256), 0)) < 0) { // Ensure not to exceed buffer size.
        printf("Reading from file %s failed: %s\n", filename, strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    // Compare the read content with the expected content.
    if (strcmp(buffer, content) != 0) {
        printf("Unexpected file content `%s`, expecting `%s`.\n", buffer, content);
        close(fd);
        return EXIT_FAILURE;
    }

    // Close the file descriptor after reading.
    close(fd);
    return EXIT_SUCCESS;
}

/// @brief Writes content to a file with specified options.
/// @param filename The name of the file to write to.
/// @param content The content to write to the file.
/// @param length The length of the content to write.
/// @param truncate Flag to indicate if the file should be truncated.
/// @param append Flag to indicate if content should be appended.
/// @return EXIT_SUCCESS on success, or EXIT_FAILURE on failure.
int write_content(const char *filename, const char *content, int length, int truncate, int append)
{
    // Set write options.
    int flags = O_WRONLY | (truncate ? O_TRUNC : append ? O_APPEND :
                                                          0);

    // Open the file with the specified flags.
    int fd = open(filename, flags, 0);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }

    // Write the content to the file.
    if (write(fd, content, length) < 0) {
        printf("Writing on file %s failed: %s\n", filename, strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    // Close the file descriptor after writing.
    close(fd);
    return EXIT_SUCCESS;
}

/// @brief Tests writing and reading operations on a file.
/// @param filename The name of the file to test.
/// @return EXIT_SUCCESS on success, or EXIT_FAILURE on failure.
int test_write_read(const char *filename)
{
    char buf[7] = { 0 }; // Buffer for reading content.

    // Create the file.
    if (create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) {
        return EXIT_FAILURE;
    }

    // Open the file for writing.
    int fd = open(filename, O_WRONLY, 0);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }

    // Write content to the file.
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

    // Close the file after writing.
    close(fd);

    // Check the content of the file.
    if (check_content(filename, "foobar", 6)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/// @brief Tests truncating and overwriting file content.
/// @param filename The name of the file to test.
/// @return EXIT_SUCCESS on success, or EXIT_FAILURE on failure.
int test_truncate(const char *filename)
{
    // Create the file.
    if (create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) {
        return EXIT_FAILURE;
    }

    // Write initial content.
    if (write_content(filename, "foobar", 6, 0, 0)) {
        return EXIT_FAILURE;
    }

    // Check the initial content.
    if (check_content(filename, "foobar", 6)) {
        return EXIT_FAILURE;
    }

    // Overwrite part of the file.
    if (write_content(filename, "bark", 4, 0, 0)) {
        return EXIT_FAILURE;
    }

    // Check the content after overwriting.
    if (check_content(filename, "barkar", 6)) {
        return EXIT_FAILURE;
    }

    // Truncate the file and write new content.
    if (write_content(filename, "barf", 4, 1, 0)) {
        return EXIT_FAILURE;
    }

    // Check the content after truncation and write.
    if (check_content(filename, "barf", 6)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/// @brief Tests appending content to a file.
/// @param filename The name of the file to test.
/// @return EXIT_SUCCESS on success, or EXIT_FAILURE on failure.
int test_append(const char *filename)
{
    // Create the file.
    if (create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) {
        return EXIT_FAILURE;
    }

    // Write initial content.
    if (write_content(filename, "fusro", 5, 0, 0)) {
        return EXIT_FAILURE;
    }

    // Append new content.
    if (write_content(filename, "dah", 3, 0, 1)) {
        return EXIT_FAILURE;
    }

    // Check the final content of the file.
    if (check_content(filename, "fusrodah", 8)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    // Specify the file name.
    char *filename = "/home/user/t_write_read.txt";

    // Test write and read operations.
    printf("Running `test_write_read`...\n");
    if (test_write_read(filename)) {
        // Clean up if there was an error.
        unlink(filename);
        return EXIT_FAILURE;
    }
    // Clean up.
    unlink(filename);

    // Test truncating and overwriting content.
    printf("Running `test_truncate`...\n");
    if (test_truncate(filename)) {
        // Clean up if there was an error.
        unlink(filename);
        return EXIT_FAILURE;
    }
    // Clean up.
    unlink(filename);

    // Test appending content.
    printf("Running `test_append`...\n");
    if (test_append(filename)) {
        // Clean up if there was an error.
        unlink(filename);
        return EXIT_FAILURE;
    }
    // Clean up.
    unlink(filename);

    return EXIT_SUCCESS;
}
