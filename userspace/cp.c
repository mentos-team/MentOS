/// @file cp.c
/// @brief `cp` program.
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <err.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("%s: missing file operand.\n", argv[0]);
        printf("Try 'cp --help' for more information.\n");
        return EXIT_FAILURE;
    }
    // Check if `--help` is provided.
    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0)) {
            printf("%s - copy files\n", argv[0]);
            printf("Usage: %s SOURCE DEST\n", argv[0]);
            return EXIT_SUCCESS;
        }
    }
    // Prepare the buffer for reading.
    ssize_t bytes_read = 0;
    char buffer[BUFSIZ];
    char *src  = argv[1];
    char *dest = argv[2];

    int srcfd = open(src, O_RDONLY, 0);
    if (srcfd < 0) {
        err(EXIT_FAILURE, "%s: %s", argv[0], src);
    }

    int destfd = creat(dest, 0600);
    if (destfd < 0) {
        err(EXIT_FAILURE, "%s: %s", argv[0], dest);
    }

    // Write the content read from srcfd to destfd
    while ((bytes_read = read(srcfd, buffer, sizeof(buffer))) > 0) {
        if (write(destfd, buffer, bytes_read) != bytes_read) {
            err(EXIT_FAILURE, "%s: %s", argv[0], dest);
        }
    }
    // Close the file descriptors.
    close(srcfd);
    close(destfd);
    return 0;
}
