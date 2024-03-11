/// @file touch.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("%s: missing operand.\n", argv[0]);
        printf("Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "--help") == 0) {
        printf("Updates modification time or creates given file.\n ");
        printf("Usage:\n");
        printf("    touch <filename>\n");
        return 0;
    }
    int fd = open(argv[1], O_RDONLY, 0);
    if (fd < 0) {
        fd = open(argv[1], O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
        if (fd >= 0) {
            close(fd);
        }
    }
    printf("\n");
    return 0;
}
