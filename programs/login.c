///// @file login.c
/// @brief Functions used to manage login.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

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

#include "ansi_colors.h"

/// Maximum length of credentials.
#define CREDENTIALS_LENGTH 50

static inline int __setup_env(passwd_t *pwd)
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

static inline void __set_io_flags(unsigned flag, bool_t active)
{
    struct termios _termios;
    tcgetattr(STDIN_FILENO, &_termios);
    if (active)
        _termios.c_lflag |= flag;
    else
        _termios.c_lflag &= ~flag;
    tcsetattr(STDIN_FILENO, 0, &_termios);
}

static inline void __print_message_file(const char * file)
{
    char buffer[256];
    ssize_t nbytes, total = 0;
    int fd;

    // Try to open the file.
    if ((fd = open(file, O_RDONLY, 0600)) == -1)
        return;
    // Read the lines of the file.
    while ((nbytes = read(fd, buffer, sizeof(char) * 256)) > 0) {
        // TODO: Parsing message files for special characters (such as `\t` for time).
        printf("%s\n", buffer);
        total += nbytes;
    }
    close(fd);
    if (total > 0)
        printf("\n");
}

/// @brief Gets the inserted command.
static inline bool_t __get_input(char *input, size_t max_len, bool_t hide)
{
    size_t index = 0;
    int c;
    bool_t result = false;

    __set_io_flags(ICANON, false);
    if (hide)
        __set_io_flags(ECHO, false);

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
                if (!hide)
                    putchar('\b');
                --index;
            }
        } else if (c == 0) {
            // Do nothing.
        } else {
            input[index++] = c;
            if (index == (max_len - 1)) {
                input[index] = 0;
                result       = true;
                break;
            }
        }
    } while (index < max_len);

    if (hide) {
        __set_io_flags(ECHO, true);
        putchar('\n');
    }
    __set_io_flags(ICANON, true);

    return result;
}

int main(int argc, char **argv)
{
    // Print /etc/issue if it exists.
    __print_message_file("/etc/issue");

    passwd_t *pwd;
    char username[CREDENTIALS_LENGTH], password[CREDENTIALS_LENGTH];
    do {
        // Get the username.
        do {
            printf("Username :");
        } while (!__get_input(username, CREDENTIALS_LENGTH, false));

        // Get the password.
        do {
            printf("Password :");
        } while (!__get_input(password, CREDENTIALS_LENGTH, true));

        // Check if we can find the user.
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

        // Check if the password is correct.
        if (strcmp(pwd->pw_passwd, password) != 0) {
            printf("Wrong password.\n");
            continue;
        }

        break;
    } while (true);

    // If there is not shell set for the user, should we rollback to standard shell?
    if (pwd->pw_shell == NULL) {
        printf("login: There is no shell set for the user `%s`.\n", pwd->pw_name);
        return 1;
    }

    // Set the standard environmental variables.
    if (!__setup_env(pwd)) {
        printf("login: Failed to setup the environmental variables.\n");
        return 1;
    }

    // Set the group id.
    setgid(pwd->pw_gid);

    // Set the user id.
    setuid(pwd->pw_uid);

    printf("\n");

    // Print /etc/motd if it exists.
    __print_message_file("/etc/motd");

    // Welcome the user.
    puts(BG_WHITE FG_BLACK);
    printf("Welcome " FG_RED "%s" FG_BLACK "...\n", pwd->pw_name);
    puts(BG_BLACK FG_WHITE_BRIGHT);

    // Call the shell.
    char *_argv[] = { pwd->pw_shell, (char *)NULL };
    if (execv(pwd->pw_shell, _argv) == -1) {
        printf("login: Failed to execute the shell.\n");
        printf("login: %s.\n", strerror(errno));
        return 1;
    }
    return 0;
}
