/// @file t_creat.c
/// @brief test the creat syscall
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/unistd.h>

int main(int argc, char *argv[])
{
    int fd = creat("foo", 0660);
    if (fd < 0) {
        printf("creat: foo: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (write(fd, "foo", 3) != 3) {
        printf("write: foo: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct stat_t st;
    if (stat("foo", &st) < 0) {
        printf("stat: foo: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (st.st_size != 3) {
        printf("Wrong file size. (expected: 3, is: %u)\n", st.st_size);
        exit(EXIT_FAILURE);
    }
    // Remove the file.
    unlink("foo");
    return 0;
}
