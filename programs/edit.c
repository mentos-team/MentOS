#include <sys/unistd.h>
#include <termios.h>
#include <stdbool.h>
#include <debug.h>
#include <stdio.h>
#include <ctype.h>

#define CTRL_KEY(k) ((k)&0x1f)

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
    enable_raw_mode();

    int c;
    do {
        c = getchar();
        if (iscntrl(c)) {
            printf("%d\n", c);
        } else {
            printf("%d ('%c')\n", c, c);
        }
    } while (c != 'q');

    disable_raw_mode();

    return 0;
}