/// @file cat.c
/// @brief `cat` program.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <strerror.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("cat: missing operand.\n");
        printf("Try 'cat --help' for more information.\n\n");
        return 1;
    }
    // Check if `--help` is provided.
    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0)) {
            printf("Prints the content of the given file.\n");
            printf("Usage:\n");
            printf("    cat <file>\n\n");
            return 0;
        }
    }
    // Prepare the buffer for reading.
    char buffer[BUFSIZ];
    // Iterate the arguments.
    for (int i = 1; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY, 42);
        if (fd < 0) {
            printf("cat: %s: %s\n\n", argv[i], strerror(errno));
            continue;
        }
        stat_t statbuf;
        if (fstat(fd, &statbuf) == -1) {
            printf("cat: %s: %s\n\n", argv[i], strerror(errno));
            // Close the file descriptor.
            close(fd);
            continue;
        }
        if (S_ISDIR(statbuf.st_mode)) {
            printf("cat: %s: %s\n\n", argv[i], strerror(EISDIR));
            // Close the file descriptor.
            close(fd);
            continue;
        }
        // Put on the standard output the characters.
        while (read(fd, buffer, BUFSIZ) > 0) {
            puts(buffer);
        }
        // Close the file descriptor.
        close(fd);
    }
    putchar('\n');
    putchar('\n');
    return 0;
}
