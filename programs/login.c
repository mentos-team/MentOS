///// @file login.c
/// @brief Functions used to manage login.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[LOGIN ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <termios.h>
#include <bits/ioctls.h>
#include <pwd.h>
#include <shadow.h>
#include <strerror.h>
#include <stdlib.h>
#include <io/ansi_colors.h>

#include <sys/mman.h>
#include <sys/wait.h>

#include <crypt/sha256.h>

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

static inline void __print_message_file(const char *file)
{
    char buffer[256];
    ssize_t nbytes, total = 0;
    int fd;

    // Try to open the file.
    if ((fd = open(file, O_RDONLY, 0600)) == -1) {
        return;
    }
    // Print the file.
    while ((nbytes = read(fd, buffer, sizeof(char) * 256)) > 0) {
        // Tap the string.
        buffer[nbytes] = 0;
        // TODO: Parsing message files for special characters (such as `\t` for time).
        write(STDOUT_FILENO, buffer, nbytes);
        total += nbytes;
    }
    close(fd);
    if (total > 0)
        printf("\n");
}

// Function to read input in a blocking manner
int read_input(char *buffer, size_t size, int show)
{
    int index = 0, c;

    do {
        c = getchar();

        // pr_crit("[%2d](%lu)\n", index, c);

        // Do nothing.
        if ((c == EOF) || (c == 0)) {
            continue;
        }

        // Newline.
        if (c == '\n') {
            if (show) {
                putchar('\n');
            }
            buffer[index] = 0;
            return index;
        }

        // Delete.
        if (c == '\b') {
            if (index > 0) {
                if (show) { putchar('\b'); }
                buffer[--index] = 0;
            }
            continue;
        }

        if (c == '\033') {
            c = getchar();
            if (c == '[') {
                // Get the char, and ignore it.
                c = getchar();

                if (c == 'D') { // LEFT
                    if (index > 0) {
                        if (show) { puts("\033[1D"); } // Move the cursor left.
                        index--;                       // Decrease index if moving left.
                    }
                } else if (c == 'C') { // RIGHT
                    if ((index < size) && (buffer[index + 1] != 0)) {
                        if (show) { puts("\033[1C"); } // Move the cursor right.
                        index++;                       // Increase index if moving right.
                    }
                }

            } else if (c == '^') {
                // Get the char.
                c = getchar();

                if (c == 'C') {
                    memset(buffer, 0, size);
                    putchar('\n');
                    return -1;
                }

                if (c == 'U') {
                    memset(buffer, 0, size);
                    if (show) {
                        // Clear the current command.
                        while (index) {
                            putchar('\b');
                            index--;
                        }
                    }
                    index = 0;
                }
            }
            continue;
        }

        if (isdigit(c) || isalpha(c)) {
            // If we moved the cursor, we need to override the character.
            if (buffer[index] && show) {
                // Move the cursor right.
                puts("\033[1C");
                // Delete the character.
                putchar('\b');
            }

            buffer[index++] = c;
            if (show) {
                putchar(c);
            }
            if (index == (size - 1)) {
                buffer[index] = 0;
                break;
            }
        }
    } while (index < size);

    return index;
}

int main(int argc, char **argv)
{
    // Print /etc/issue if it exists.
    __print_message_file("/etc/issue");

    struct termios _termios;
    tcgetattr(STDIN_FILENO, &_termios);
    _termios.c_lflag &= ~ISIG;
    tcsetattr(STDIN_FILENO, 0, &_termios);

    passwd_t *pwd;
    char username[CREDENTIALS_LENGTH], password[CREDENTIALS_LENGTH];
    do {
        __set_echo(false);
        // Get the username and password.

        do {
            puts("Username: ");
        } while (read_input(username, CREDENTIALS_LENGTH, 1) <= 0);

        printf("Password: ");
        if (read_input(password, CREDENTIALS_LENGTH, 0) < 0) {
            fprintf(stderr, "Error reading password\n");
            return EXIT_FAILURE;
        }
        putchar('\n');

        __set_echo(true);

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

        struct spwd *spwd;
        if ((spwd = getspnam(username)) == NULL) {
            printf("Could not retrieve the secret password of %s:%s\n", username, strerror(errno));
            continue;
        }

        unsigned char hash[SHA256_BLOCK_SIZE]       = { 0 };
        char hash_string[SHA256_BLOCK_SIZE * 2 + 1] = { 0 };
        SHA256_ctx_t ctx;
        sha256_init(&ctx);
        for (unsigned i = 0; i < 100000; ++i)
            sha256_update(&ctx, (unsigned char *)password, strlen(password));
        sha256_final(&ctx, hash);
        sha256_bytes_to_hex(hash, SHA256_BLOCK_SIZE, hash_string, SHA256_BLOCK_SIZE * 2 + 1);

        // Check if the password is correct.
        if (strcmp(spwd->sp_pwdp, hash_string) != 0) {
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
    if (setgid(pwd->pw_gid) < 0) {
        printf("login: Failed to change group id: %s\n", strerror(errno));
        return 1;
    }

    // Set the user id.
    if (setuid(pwd->pw_uid) < 0) {
        printf("login: Failed to change user id: %s\n", strerror(errno));
        return 1;
    }

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
