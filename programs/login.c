/////                MentOS, The Mentoring Operating system project
/// @file login.c
/// @brief Functions used to manage login.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

/// Maximum length of credentials.
#include <string.h>
#include <stdbool.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <termios.h>
#include <bits/ioctls.h>
#include <pwd.h>
#include <strerror.h>
#include <stdlib.h>

#include <debug.h>

#define CREDENTIALS_LENGTH 50

#define FG_BLACK "\033[30m"
#define FG_WHITE "\033[37m"
#define FG_RED   "\033[31m"
#define BG_WHITE "\033[47m"
#define BG_BLACK "\033[40m"

void set_echo(bool_t active)
{
    struct termios _termios;
    tcgetattr(STDIN_FILENO, &_termios);
    if (active)
        _termios.c_lflag |= (ICANON | ECHO);
    else
        _termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, 0, &_termios);
}

void set_erase(bool_t active)
{
    struct termios _termios;
    tcgetattr(STDIN_FILENO, &_termios);
    if (active)
        _termios.c_lflag |= ECHOE;
    else
        _termios.c_lflag &= ~ECHOE;
    tcsetattr(STDIN_FILENO, 0, &_termios);
}

/// @brief Gets the inserted command.
static bool_t get_input(char *input, size_t max_len, bool_t hide)
{
    size_t index = 0;
    int c;
    bool_t result = false;

    set_erase(false);
    if (hide) {
        set_echo(false);
    }

    memset(input, 0, max_len);
    do {
        c = getchar();
        // Return Key
        if (c == '\n') {
            input[index] = 0;
            result       = true;
            break;
        } else if (c == '\033') {
            c = getchar();
            if (c == '[') {
                c = getchar(); // Get the char, and ignore it.
            } else if (c == '^') {
                c = getchar(); // Get the char.
                if (c == 'C') {
                    // However, the ISR of the keyboard has already put the char.
                    // Thus, delete it by using backspace.
                    if (!hide) {
                        putchar('\b');
                        putchar('\b');
                        putchar('\n');
                    }
                    result = false;
                    break;
                } else if (c == 'U') {
                    if (!hide) {
                        // However, the ISR of the keyboard has already put the char.
                        // Thus, delete it by using backspace.
                        putchar('\b');
                        putchar('\b');
                        // Clear the current command.
                        for (size_t it = 0; it < index; ++it) {
                            putchar('\b');
                        }
                    }
                    index = 0;
                }
            }
        } else if (c == '\b') {
            if (index > 0) {
                if (!hide) {
                    putchar('\b');
                }
                --index;
            }
        } else {
            input[index++] = c;
            if (index == (max_len - 1)) {
                input[index] = 0;
                result       = true;
                break;
            }
        }
    } while (index < max_len);

    set_erase(true);
    if (hide) {
        set_echo(true);
        putchar('\n');
    }

    return result;
}

static inline int setup_env(passwd_t *pwd)
{
    // Set the USER.
    if (setenv("USER", pwd->pw_name, 1) == -1) {
        printf("Failed to set env: `USER`\n");
        return 0;
    }
    // Set the SHELL.
    if (setenv("SHELL", pwd->pw_shell, 1) == -1) {
        printf("Failed to set env: `SHELL`\n");
        return 0;
    }
    // Set the HOME.
    if (setenv("HOME", pwd->pw_dir, 1) == -1) {
        printf("Failed to set env: `HOME`\n");
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    passwd_t *pwd;
    char username[50], password[50];
    do {
        // Get the username.
        do {
            printf("Username :");
        } while (!get_input(username, 50, false));
        do {
            printf("Password :");
        } while (!get_input(password, 50, true));
        if ((pwd = getpwnam(username)) == NULL) {
            if (errno == ENOENT) {
                printf("The given name was not found.\n");
            } else if (errno == 0) {
                printf("Cannot access passwd file.\n");
            } else {
                printf("Unknown error (%s).\n", strerror(errno));
            }
            continue;
        }
        if (strcmp(pwd->pw_passwd, password) != 0) {
            printf("Wrong password.\n");
            continue;
        }
        break;
    } while (true);
    if (pwd->pw_shell == NULL) {
        printf("%s: There is no shell set for the user `%s`.\n", argv[0], pwd->pw_name);
        return 1;
    }

    if (!setup_env(pwd)) {
        printf("%s: Failed to setup the environmental variables.\n", argv[0]);
        return 1;
    }

    char *_argv[] = { pwd->pw_shell, (char *)NULL };
    if (execv(pwd->pw_shell, _argv) == -1) {
        printf("%s: Failed to execute the shell.\n", argv[0]);
        printf("%s: %s.\n", argv[0], strerror(errno));
        return 1;
    }
    return 0;
}