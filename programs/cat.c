///                MentOS, The Mentoring Operating system project
/// @file cat.c
/// @brief `cat` program.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <strerror.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("%s: missing operand.\n", argv[0]);
        printf("Try '%s --help' for more information.\n\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "--help") == 0) {
        printf("Prints the content of the given file.\n");
        printf("Usage:\n");
        printf("    %s <file>\n\n", argv[0]);
        return 0;
    }
    int fd = open(argv[1], O_RDONLY, 42);
    if (fd < 0) {
        printf("%s: %s: %s\n\n", argv[0], strerror(errno), argv[1]);
        return 1;
    }
    char buffer[BUFSIZ];
    // Put on the standard output the characters.
    while (read(fd, buffer, BUFSIZ) > 0) {
        puts(buffer);
    }
    putchar('\n');
    putchar('\n');
    close(fd);
    return 0;
}
