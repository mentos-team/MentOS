/// @file cat.c
/// @brief `cat` program.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "io/debug.h"
#include "stddef.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <strerror.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("cat: missing operand.\n");
        printf("Try 'cat --help' for more information.\n");
        return 1;
    }
    // Check if `--help` is provided.
    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0)) {
            printf("Print the content of each given file.\n");
            printf("Usage:\n");
            printf("    cat <file>\n");
            return 0;
        }
    }
    int ret = 0;
    int fd;
    // Prepare the buffer for reading.
    char buffer[BUFSIZ];
    // Iterate the arguments.
    for (int i = 1; i < argc; ++i) {
        // Initialize the file path.
        char *filepath = argv[i];

        fd = open(filepath, O_RDONLY, 0);
        if (fd < 0) {
            printf("cat: %s: %s\n", filepath, strerror(errno));
            ret = EXIT_FAILURE;
            continue;
        }
        ssize_t bytes_read = 0;
        // Put on the standard output the characters.
        while ((bytes_read = read(fd, buffer, BUFSIZ)) > 0) {
            write(STDOUT_FILENO, buffer, bytes_read);
        }
        close(fd);
        if (bytes_read < 0) {
            printf("%s: %s: %s\n", argv[0], filepath, strerror(errno));
            ret = EXIT_FAILURE;
        }
    }
    return ret;
}
