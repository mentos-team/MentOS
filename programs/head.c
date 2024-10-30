/// @file head.c
/// @brief `head` program.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <err.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

static int head(int fd, const char *fname, size_t n) {
    // Count the printed lines
    int i = 0;
    // Prepare the buffer for reading.
    char buffer[BUFSIZ];
    size_t leftover = 0;
    char *line = buffer;
    ssize_t bytes_read = 0;

    while ((bytes_read = read(fd, buffer + leftover, sizeof(buffer) - leftover)) > 0) {
        char *lineend;
        while (i < n && (lineend = memchr(line, '\n', sizeof(buffer) - (line - buffer)))) {
            lineend++; // Include the newline
            write(STDOUT_FILENO, line, lineend-line);
            line = lineend;
            i++;
        }

        if (i >= n)
            break;

        leftover = (leftover + bytes_read) - (line - buffer); // Bytes left in the buffer
        memmove(buffer, line, leftover); // Move the leftover to the front of buffer
        line = buffer;
    }

    close(fd);
    if (bytes_read < 0) {
        fprintf(STDERR_FILENO, "head: %s: %s\n", fname, strerror(errno));
        return EXIT_FAILURE;
    }

    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    int n = 10;
    char **file_args_start = argv;

    // Find help argument
    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0)) {
            printf("Print the first part of files.\n");
            printf("Usage:\n");
            printf("    head [-<num>] [FILE]...\n");
            return 0;
        }
    }

    // Detect number of lines argument
    if (argv[1][0] == '-') {
        char *nptr = argv[1]+1;
        char *endptr;
        errno = 0;
        n = strtol(nptr, &endptr, 10);
           if (*endptr || errno == ERANGE) {
               errx(EXIT_FAILURE, "head: invalid number of lines: `%s`", nptr);
           }

           if (endptr == nptr) {
               errx(EXIT_FAILURE, "head: line number option requires an argument");
           }
        file_args_start = &argv[1];
        argc--;
    }

    // No argument was provided -> read from stdin
    if (argc == 1) {
        return head(STDIN_FILENO, "stdin", n);
    }

    for (int i = 1; i < argc; i++) {
        const char* fname = file_args_start[i];
        int fd;
        if (strcmp(fname, "-") == 0) {
            fd = STDIN_FILENO;
        } else {
            fd = open(fname, O_RDONLY, 0);
            if (fd < 0) {
                fprintf(STDERR_FILENO, "head: %s: %s\n", fname, strerror(errno));
                ret = EXIT_FAILURE;
                continue;
            }
        }

        // Print file header if multiple files are given
        if (argc > 2) {
            printf("==> %s <==\n", fname);
        }

        ret = ret || head(fd, fname, n);
    }
    return ret;
}
