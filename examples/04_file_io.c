/// @file 04_file_io.c
/// @brief Fourth example: File I/O operations
/// @details This program demonstrates:
/// - Opening files with open()
/// - Reading from files with read()
/// - Writing to files with write()
/// - File descriptors and flags (O_RDONLY, O_WRONLY, O_CREAT)
/// - Proper resource cleanup
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    const char *filename = "/home/user/example_output.txt";
    const char *message  = "This file was created by 04_file_io example!\n"
                          "It demonstrates basic file I/O in MentOS.\n";

    printf("Creating file: %s\n", filename);

    // Open file for writing, create if doesn't exist, truncate if exists
    // Mode 0644 gives owner read/write, group/others read
    int fd_write = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd_write < 0) {
        perror("open (write)");
        return EXIT_FAILURE;
    }

    printf("Writing data to file...\n");
    ssize_t bytes_written = write(fd_write, message, strlen(message));

    if (bytes_written < 0) {
        perror("write");
        close(fd_write);
        return EXIT_FAILURE;
    }

    printf("Wrote %ld bytes\n", bytes_written);
    close(fd_write);

    // Now read the file back
    printf("Reading file back...\n");
    int fd_read = open(filename, O_RDONLY, 0);

    if (fd_read < 0) {
        perror("open (read)");
        return EXIT_FAILURE;
    }

    char buffer[512];
    ssize_t bytes_read = read(fd_read, buffer, sizeof(buffer) - 1);

    if (bytes_read < 0) {
        perror("read");
        close(fd_read);
        return EXIT_FAILURE;
    }

    buffer[bytes_read] = '\0'; // Null-terminate
    printf("File contents:\n---\n%s---\n", buffer);

    close(fd_read);
    printf("Done!\n");

    return EXIT_SUCCESS;
}
