/// @file t_dup.c
/// @brief Test the dup syscall
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <strerror.h>
#include <string.h>

int main(int argc, char *argv[])
{
    char *file        = "t_dup_file";
    int fd1, fd2;
    int flags = O_WRONLY | O_CREAT | O_TRUNC;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

    fd1 = open(file, flags, mode);
    if (fd1 < 0) {
        printf("Failed to open file %s: %s\n", file, strerror(errno));
        exit(1);
    }

    fd2 = dup(fd1);
    if (fd2 < 0) {
        printf("Failed to dup fd %d: %s\n", fd1, strerror(errno));
        exit(1);
    }

    if (write(fd1, "foo", 3) != 3) {
        printf("Writing to fd %d failed: %s\n", fd1, strerror(errno));
        exit(1);
    }
    close(fd1);

    if (write(fd2, "bar", 3) != 3) {
        printf("Writing to fd %d failed: %s\n", fd2, strerror(errno));
        exit(1);
    }
    close(fd2);

    fd1 = open(file, O_RDONLY, 0);
    if (fd1 < 0) {
        printf("Failed to open file %s: %s\n", file, strerror(errno));
        exit(1);
    }

    char buf[7];
    buf[6] = 0;
    if (read(fd1, &buf, 6) < 0) {
        printf("Reading from fd %d failed: %s\n", fd1, strerror(errno));
        exit(1);
    }

    if (strcmp(buf, "foobar") != 0) {
        printf("Unexpected file content: %s\n", buf);
        exit(1);
    }

    unlink(file);
    return 0;
}
