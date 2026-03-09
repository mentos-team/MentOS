/// @file t_dup.c
/// @brief Test the dup syscall.
/// @details This program tests the `dup` system call by duplicating a file
/// descriptor, writing to both descriptors, reading the content, and verifying
/// the result.
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

int main(int argc, char *argv[])
{
    char *filename = "/home/user/t_dup.txt";
    int fd1;
    int fd2;
    int flags   = O_WRONLY | O_CREAT | O_TRUNC;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

    // Open the file with specified flags and mode.
    fd1 = open(filename, flags, mode);
    if (fd1 < 0) {
        syslog(LOG_ERR, "[t_dup] Failed to open file %s: %s\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Duplicate the file descriptor.
    fd2 = dup(fd1);
    if (fd2 < 0) {
        syslog(LOG_ERR, "[t_dup] Failed to dup fd %d: %s\n", fd1, strerror(errno));
        // Close the file descriptor.
        if (close(fd1) < 0) {
            syslog(LOG_ERR, "[t_dup] close fd1: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Write "foo" to the first file descriptor.
    if (write(fd1, "foo", 3) != 3) {
        syslog(LOG_ERR, "[t_dup] Writing to fd %d failed: %s\n", fd1, strerror(errno));
        // Close the file descriptor.
        if (close(fd1) < 0) {
            syslog(LOG_ERR, "[t_dup] close fd1: %s: %s\n", filename, strerror(errno));
        }
        if (close(fd2) < 0) {
            syslog(LOG_ERR, "[t_dup] close fd2: %s: %s\n", filename, strerror(errno));
        }
        // Remove the file.
        if (unlink(filename) < 0) {
            syslog(LOG_ERR, "[t_dup] unlink: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Close the file descriptor.
    if (close(fd1) < 0) {
        // Error handling for close failure.
        syslog(LOG_ERR, "[t_dup] close fd1: %s: %s\n", filename, strerror(errno));
        // Remove the file.
        if (unlink(filename) < 0) {
            syslog(LOG_ERR, "[t_dup] unlink: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Write "bar" to the duplicated file descriptor.
    if (write(fd2, "bar", 3) != 3) {
        // Error handling for write failure.
        syslog(LOG_ERR, "[t_dup] Writing to fd %d failed: %s\n", fd2, strerror(errno));
        // Close the file descriptor.
        if (close(fd2) < 0) {
            syslog(LOG_ERR, "[t_dup] close fd2: %s: %s\n", filename, strerror(errno));
        }
        // Remove the file.
        if (unlink(filename) < 0) {
            syslog(LOG_ERR, "[t_dup] unlink: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Close the file descriptor.
    if (close(fd2) < 0) {
        // Error handling for close failure.
        syslog(LOG_ERR, "[t_dup] close fd2: %s: %s\n", filename, strerror(errno));
        // Remove the file.
        if (unlink(filename) < 0) {
            syslog(LOG_ERR, "[t_dup] unlink: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Reopen the file for reading.
    fd1 = open(filename, O_RDONLY, mode);
    if (fd1 < 0) {
        // Error handling for file open failure.
        syslog(LOG_ERR, "[t_dup] Failed to open file %s: %s\n", filename, strerror(errno));
        // Remove the file.
        if (unlink(filename) < 0) {
            syslog(LOG_ERR, "[t_dup] unlink: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Buffer to read the file content.
    char buf[7] = {0};

    // Read the content of the file.
    if (read(fd1, buf, 6) < 0) {
        // Error handling for read failure.
        syslog(LOG_ERR, "[t_dup] Reading from fd %d failed: %s\n", fd1, strerror(errno));
        // Close the file descriptor.
        if (close(fd1) < 0) {
            syslog(LOG_ERR, "[t_dup] close fd1: %s: %s\n", filename, strerror(errno));
        }
        // Remove the file.
        if (unlink(filename) < 0) {
            syslog(LOG_ERR, "[t_dup] unlink: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Close the file descriptor.
    if (close(fd1) < 0) {
        // Error handling for close failure.
        syslog(LOG_ERR, "[t_dup] close fd1: %s: %s\n", filename, strerror(errno));
        // Remove the file.
        if (unlink(filename) < 0) {
            syslog(LOG_ERR, "[t_dup] unlink: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Check if the file content is as expected.
    if (strcmp(buf, "foobar") != 0) {
        syslog(LOG_ERR, "[t_dup] Unexpected file content: %s\n", buf);
        // Remove the file.
        if (unlink(filename) < 0) {
            syslog(LOG_ERR, "[t_dup] unlink: %s: %s\n", filename, strerror(errno));
        }
        exit(EXIT_FAILURE);
    }

    // Remove the file.
    if (unlink(filename) < 0) {
        // Error handling for unlink failure.
        syslog(LOG_ERR, "[t_dup] Failed to delete file %s: %s\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    return 0;
}
