/// @file more.c
/// @brief `more` program.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stdbool.h"
#include "stddef.h"
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Video dimensions defined in mentos/src/io/video.c
#define HEIGHT    25
#define WIDTH     80
#define LAST_LINE (HEIGHT - 1)

static void erase_backwards(int n)
{
    for (int i = 0; i < n; i++) {
        putchar('\b');
    }
}

static void page_content(int fd)
{
    int lines = 0;
    char line[WIDTH + 2];
    char *lineend;
    while ((lineend = fgets(line, WIDTH, fd))) {
        if (lineend - line == WIDTH && line[WIDTH - 1] != '\n') {
            line[WIDTH - 1] = '+';
            line[WIDTH]     = '\n';
            line[WIDTH + 1] = 0;
        }

        printf("%s", line);

        lines++;
        if (lines == LAST_LINE) {
            int prompt = printf("--more--");
            int c;
            bool_t quit = false;
            do {
                c = getchar();
                if (c == 'q') {
                    quit = true;
                } else if (c == '\n') {
                    lines = LAST_LINE - 1;
                } else if (c == ' ') {
                    lines = 0;
                } else {
                    continue;
                }

                break;
            } while (1);
            erase_backwards(prompt);
            if (quit) {
                exit(EXIT_SUCCESS);
            }
        }
    }
}

int main(int argc, char **argv)
{
    // Check if `--help` is provided.
    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0)) {
            printf("Display the content of a file.\n");
            printf("Usage:\n");
            printf("    more [FILE]\n");
            return 0;
        }
    }
    char buffer[PATH_MAX];
    char *filepath = argv[1];

    struct termios _termios;
    tcgetattr(STDIN_FILENO, &_termios);
    _termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, 0, &_termios);

    int fd = STDOUT_FILENO;
    if (argc > 1) {
        fd = open(filepath, O_RDONLY, 0);
        if (fd < 0) {
            printf("more: %s: %s\n", filepath, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    errno = 0;
    page_content(fd);
    if (errno) {
        printf("%s: %s: %s\n", argv[0], argc > 1 ? filepath : "stdin", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return 0;
}
