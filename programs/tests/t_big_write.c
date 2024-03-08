/// @file t_big_write.c
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief Test writing a big file.
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

int main(int argc, char *argv[])
{
    int flags               = O_WRONLY | O_CREAT | O_TRUNC;
    mode_t mode             = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    char *filename          = "/home/user/test.txt";
    char buffer[4 * BUFSIZ] = { 0 };
    // Open the file.
    int fd = open(filename, flags, mode);
    if (fd < 0) {
        printf("Failed to open file %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }
    // Write the content.
    for (unsigned times = 0; times < 128; ++times) {
        for (unsigned i = 'A'; i < 'z'; ++i) {
            memset(buffer, i, 4 * BUFSIZ);
            if (write(fd, buffer, 4 * BUFSIZ) < 0) {
                printf("Writing on file %s failed: %s\n", filename, strerror(errno));
                close(fd);
                unlink(filename);
                return EXIT_FAILURE;
            }
        }
    }
    close(fd);
    // unlink(filename);
    return EXIT_SUCCESS;
}
