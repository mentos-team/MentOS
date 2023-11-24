/// @file cat.c
/// @brief `cat` program.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "io/debug.h"
#include "stddef.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <strerror.h>
#include <sys/stat.h>

static inline void print_content(const char *path, char *buffer, unsigned buflen)
{
    pr_warning("Printing content of %s\n", path);
    // Open the file.
    int fd = open(path, O_RDONLY, 42);
    if (fd >= 0) {
        // Put on the standard output the characters.
        while (read(fd, buffer, buflen) > 0) {
            puts(buffer);
        }
        // Close the file descriptor.
        close(fd);
    } else {
        printf("cat: %s: %s\n\n", path, strerror(errno));
    }
}

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
        stat_t statbuf;
        if (stat(argv[i], &statbuf) == -1) {
            printf("cat: %s: %s\n\n", argv[i], strerror(errno));
            continue;
        }
        // If it is a regular file, just print the content.
        if (S_ISREG(statbuf.st_mode)) {
            print_content(argv[i], buffer, BUFSIZ);

        } else if (S_ISDIR(statbuf.st_mode)) {
            printf("cat: %s: Is a directory\n\n", argv[i]);

        } else if (S_ISLNK(statbuf.st_mode)) {
            if (readlink(argv[i], buffer, BUFSIZ)) {
                print_content(buffer, buffer, BUFSIZ);
            } else {
                printf("cat: %s: %s\n\n", argv[i], strerror(errno));
            }
        }
    }
    putchar('\n');
    putchar('\n');
    return 0;
}
