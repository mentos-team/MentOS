#include <sys/unistd.h>
#include <sys/stat.h>
#include <strerror.h>
#include <termios.h>
#include <stdbool.h>
#include <string.h>
#include <debug.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>

#define CTRL_KEY(k) ((k)&0x1f)
#define BUFFER_SIZE 4096

static struct termios _original_termios, _termios;

static inline void die(const char *s)
{
    printf("error: %s\n", s);
    exit(1);
}

static inline void disable_raw_mode()
{
    tcsetattr(STDIN_FILENO, 0, &_original_termios);
}

static inline void enable_raw_mode()
{
    tcgetattr(STDIN_FILENO, &_original_termios);
    _termios = _original_termios;
    _termios.c_lflag &= ~(ICANON | ECHO | IEXTEN | ISIG);
    tcsetattr(STDIN_FILENO, 0, &_termios);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("edit: missing operand.\n");
        printf("Try 'edit --help' for more information.\n\n");
        return 1;
    }
    if (strcmp(argv[1], "--help") == 0) {
        printf("Prints the content of the given file.\n");
        printf("Usage:\n");
        printf("    edit <file>\n\n");
        return 0;
    }
    int fd = open(argv[1], O_RDONLY, 42);
    if (fd < 0) {
        printf("edit: %s: %s\n\n", argv[0], strerror(errno));
        return 1;
    }
    stat_t statbuf;
    if (fstat(fd, &statbuf) == -1) {
        printf("edit: %s: %s\n\n", argv[0], strerror(errno));
        // Close the file descriptor.
        close(fd);
        return 1;
    }
    if (S_ISDIR(statbuf.st_mode)) {
        printf("edit: %s: %s\n\n", argv[0], strerror(EISDIR));
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

    enable_raw_mode();

    int c;
    do {
        c = getchar();
        // Return Key
        if (c == '\n') {
            putchar('\n');
        } else if (c == '\033') {
            c = getchar();
            if (c == '[') {
                c = getchar();
                if (c == 'D') {
                    puts("\033[1D");
                } else if (c == 'C') {
                    puts("\033[1C");
                } else if (c == 'H') {
                    printf("\033[%dD", 0);
                } else if (c == 'F') {
                    printf("\033[%dC", 60);
                }
            }
        } else {
            putchar(c);
        }
    } while (c != 'q');

    disable_raw_mode();
    return 0;
}