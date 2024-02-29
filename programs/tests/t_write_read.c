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

int main(int argc, char *argv[])
{
    char *file = "t_write_read_file";
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

    int fd = creat(file, mode);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", file, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (write(fd, "foo", 3) != 3) {
        printf("First write to %s failed: %s\n", file, strerror(errno));
        exit(1);
    }

    if (write(fd, "bar", 3) != 3) {
        printf("Second write to %s failed: %s\n", file, strerror(errno));
        exit(1);
    }

    close(fd);

    fd = open(file, O_RDONLY, 0);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", file, strerror(errno));
        exit(1);
    }

    ssize_t bytes_read;
    char buf[7];
    buf[6] = 0;
    if ((bytes_read = read(fd, &buf, 6)) < 0) {
        printf("Reading from file %s failed: %s\n", file, strerror(errno));
        exit(1);
    }

    if (strcmp(buf, "foobar") != 0) {
        printf("Unexpected file content: %s\n", buf);
        exit(1);
    }

    unlink(file);
    return 0;
}
