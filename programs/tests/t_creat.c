/// @file t_creat.c
/// @brief Test the creat syscall.
/// @details This program tests the `creat` system call by creating a file,
/// writing to it, checking its size, and then removing it.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    char *filename      = "/home/user/t_creat.txt";
    char *content       = "Hello world!";
    size_t content_size = strlen(content);

    // Create the file with read and write permissions for the owner and group.
    int fd = creat(filename, 0660);
    if (fd < 0) {
        // Error handling for file creation failure.
        fprintf(STDERR_FILENO, "creat: %s: %s\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Write the string to the file.
    if (write(fd, content, content_size) != content_size) {
        // Error handling for write failure.
        fprintf(STDERR_FILENO, "write: %s: %s\n", filename, strerror(errno));
        // Close the file descriptor.
        if (close(fd) < 0) {
            fprintf(STDERR_FILENO, "close: %s: %s\n", filename, strerror(errno));
        }
        // Remove the file.
        if (unlink(filename) < 0) {
            fprintf(STDERR_FILENO, "unlink: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Close the file descriptor.
    if (close(fd) < 0) {
        // Error handling for close failure.
        fprintf(STDERR_FILENO, "close: %s: %s\n", filename, strerror(errno));
        // Remove the file.
        if (unlink(filename) < 0) {
            fprintf(STDERR_FILENO, "unlink: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Structure to hold file status information.
    struct stat st;
    // Get the status of the file filename.
    if (stat(filename, &st) < 0) {
        // Error handling for stat failure.
        fprintf(STDERR_FILENO, "stat: %s: %s\n", filename, strerror(errno));
        // Remove the file.
        if (unlink(filename) < 0) {
            fprintf(STDERR_FILENO, "unlink: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Check if the file size is correct.
    if (st.st_size != content_size) {
        fprintf(STDERR_FILENO, "Wrong file size. (expected: %ld, is: %ld)\n", content_size, st.st_size);
        // Remove the file.
        if (unlink(filename) < 0) {
            fprintf(STDERR_FILENO, "unlink: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Remove the file.
    if (unlink(filename) < 0) {
        // Error handling for unlink failure.
        fprintf(STDERR_FILENO, "unlink: %s: %s\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
