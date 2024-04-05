/// @file edit.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <sys/stat.h>
#include <strerror.h>
#include <termios.h>
#include <stdbool.h>
#include <string.h>
#include <io/debug.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>

#define CTRL_KEY(k) ((k)&0x1f)
#define BUFFER_SIZE 4096

static inline void __set_echo(bool_t active)
{
    struct termios _termios;
    tcgetattr(STDIN_FILENO, &_termios);
    if (active) {
        _termios.c_lflag |= (ICANON | ECHO);
    } else {
        _termios.c_lflag &= ~(ICANON | ECHO);
    }
    tcsetattr(STDIN_FILENO, 0, &_termios);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("edit: missing operand.\n");
        printf("Try 'edit --help' for more information.\n");
        return 1;
    }
    if (strcmp(argv[1], "--help") == 0) {
        printf("Prints the content of the given file.\n");
        printf("Usage:\n");
        printf("    edit <file>\n");
        return 0;
    }
    int fd = open(argv[1], O_RDONLY, 42);
    if (fd < 0) {
        printf("edit: %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    stat_t statbuf;
    if (fstat(fd, &statbuf) == -1) {
        printf("edit: %s: %s\n", argv[1], strerror(errno));
        // Close the file descriptor.
        close(fd);
        return 1;
    }
    if (S_ISDIR(statbuf.st_mode)) {
        printf("edit: %s: %s\n", argv[1], strerror(EISDIR));
        // Close the file descriptor.
        close(fd);
        return 1;
    }
    // Put on the standard output the characters.
    char buffer[BUFFER_SIZE];
    while (read(fd, buffer, BUFFER_SIZE) > 0) {
        puts(buffer);
    }
    // Close the file descriptor.
    close(fd);

    struct termios _termios;
    tcgetattr(STDIN_FILENO, &_termios);
    _termios.c_lflag &= ~ISIG;
    tcsetattr(STDIN_FILENO, 0, &_termios);

    __set_echo(false);

    do {
        int c = getchar();
        // Return Key
        if (c == '\n') {
            putchar('\n');
            // Break the while loop.
            continue;
        } else if (c == '\033') {
            c = getchar();
            if (c == '[') {
                c = getchar(); // Get the char.
                if ((c == 'A') || (c == 'B')) {
                    pr_debug("UP\n");
                } else if (c == 'D') {
                    pr_debug("RIGHT\n");
                } else if (c == 'C') {
                    pr_debug("LEFT\n");
                } else if (c == 'H') {
                    pr_debug("HOME\n");
                } else if (c == 'F') {
                    pr_debug("END\n");
                } else if (c == '3') {
                    pr_debug("NO-IDEA\n");
                }
            }
        } else if (c == '\b') {
            putchar('\b');
        } else if (c == '\t') {
            pr_debug("TAB\n");
        } else if (c == 127) {
            pr_debug("127\n");
            putchar(127);
        } else if (iscntrl(c)) {
            if (c == CTRL('C')) {
                pr_debug("CTRL(C)\n");
                break;
            } else if (c == CTRL('U')) {
                pr_debug("CTRL(U)\n");
            } else if (c == CTRL('D')) {
                pr_debug("CTRL(D)\n");
            }
        } else if ((c > 0) && (c != '\n')) {
            putchar(c);
        } else {
            pr_debug("Unrecognized character %02x (%c)\n", c, c);
        }
    } while (true);

    __set_echo(true);
    return 0;
}
