///// @file login.c
/// @brief Functions used to manage login.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[LOGIN ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

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

/// @brief Prints the contents of a message file to standard output.
/// @param file The path to the file to be printed.
static inline void __print_message_file(const char *file)
{
    char buffer[256]; ///< Buffer to hold file contents
    ssize_t nbytes;   ///< Number of bytes read from the file
    int fd;           ///< File descriptor for the opened file

    // Try to open the file in read-only mode
    if ((fd = open(file, O_RDONLY, 0600)) == -1) {
        perror("Error opening file"); // Print error if file can't be opened
        return;
    }

    // Read and print the file contents
    while ((nbytes = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[nbytes] = '\0'; // Null-terminate the string
        // TODO: Parsing message files for special characters (such as `\t` for time).
        write(STDOUT_FILENO, buffer, nbytes); // Write to standard output.
    }

    // Close the file descriptor
    close(fd);

    // Print a newline if any content was read
    if (nbytes > 0) {
        printf("\n");
    }
}

/// @brief Reads user input into a buffer, supporting basic editing features.
/// @param buffer The buffer to store the input string.
/// @param size The maximum size of the buffer.
/// @param show Flag to determine if input should be displayed.
/// @return The length of the input read, or -1 if a special command (Ctrl+C) is
/// detected.
static inline int __read_input(char *buffer, size_t size, int show)
{
    int index = 0, c, length = 0, insert_active = 0;

    // Clear the buffer at the start
    memset(buffer, 0, size);

    do {
        c = getchar(); // Read a character from input

        pr_debug("%c\n", c);

        // Ignore EOF and null or tab characters
        if (c == EOF || c == 0 || c == '\t') {
            continue;
        }

        // Handle newline character to finish input
        if (c == '\n') {
            if (show) {
                putchar('\n'); // Display a newline
            }
            return length; // Return length of input
        }

        // Handle backspace for deletion
        if (c == '\b') {
            if (index > 0) {
                --length; // Decrease length
                --index;  // Move index back
                // Shift the buffer left to remove the character
                memmove(buffer + index, buffer + index + 1, length - index + 1);
                if (show) { putchar('\b'); } // Show backspace action
            }
            continue;
        }

        // Handle space character
        if (c == ' ') {
            // Shift buffer to the right to insert space
            memmove(buffer + index + 1, buffer + index, length - index + 1);
            buffer[index++] = c; // Insert space
            length++;
            if (show) { putchar(c); } // Show space
            continue;
        }

        // Handle escape sequences (for arrow keys, home, end, etc.)
        if (c == '\033') {
            c = getchar(); // Get the next character
            if (c == '[') {
                // Get the direction key (Left, Right, Home, End, Insert, Delete)
                c = getchar();

                if (c == 'D') { // LEFT Arrow
                    if (index > 0) {
                        if (show) { puts("\033[1D"); } // Move the cursor left
                        index--;                       // Decrease index
                    }
                } else if (c == 'C') { // RIGHT Arrow
                    if (index < length) {
                        if (show) { puts("\033[1C"); } // Move the cursor right
                        index++;                       // Increase index
                    }
                } else if (c == '1') {                                // HOME
                    if (show) { printf("\033[%dD", index); }          // Move cursor to the beginning
                    index = 0;                                        // Set index to the start
                } else if (c == '4') {                                // END
                    if (show) { printf("\033[%dC", length - index); } // Move cursor to the end
                    index = length;                                   // Set index to the end
                } else if (c == '2') {                                // INSERT
                    insert_active = !insert_active;                   // Toggle insert mode
                } else if (c == '3') {                                // DELETE
                    if (index < length) {
                        --length;                    // Decrease length
                        if (show) { putchar(0x7F); } // Show delete character
                        // Shift left to remove character at index
                        memmove(buffer + index, buffer + index + 1, length - index + 1);
                    }
                }

            } else if (c == '^') {
                // Handle special commands (Ctrl+C, Ctrl+U)
                c = getchar();
                if (c == 'C') {
                    memset(buffer, 0, size); // Clear buffer
                    putchar('\n');
                    return -1; // Return -1 on Ctrl+C
                }

                if (c == 'U') {
                    memset(buffer, 0, size); // Clear the current command
                    if (show) {
                        // Clear the current command from display
                        while (index--) {
                            putchar('\b'); // Move cursor back
                        }
                    }
                    index = 0; // Reset index
                }
            }
            continue;
        }

        // Handle alphanumeric input
        if (isdigit(c) || isalpha(c)) {
            // Handle insertion based on insert mode
            if (!insert_active) {
                // Shift buffer to the right to insert new character
                memmove(buffer + index + 1, buffer + index, length - index + 1);
            } else if (show && (index < length - 1)) {
                puts("\033[1C"); // Move cursor right
                putchar('\b');   // Prepare to delete the character
            }

            buffer[index++] = c; // Insert new character
            length++;            // Increase length

            if (show) { putchar(c); } // Show new character

            // Check if we reached the buffer limit
            if (index == (size - 1)) {
                buffer[index] = 0; // Null-terminate the buffer
                break;             // Exit loop if buffer is full
            }
        }
    } while (length < size);

    return length; // Return total length of input
}

int main(int argc, char **argv)
{
    // Print /etc/issue if it exists.
    __print_message_file("/etc/issue");

    passwd_t *pwd;
    char username[CREDENTIALS_LENGTH], password[CREDENTIALS_LENGTH];
    struct termios _termios;

    do {
        // Get terminal attributes for input handling
        tcgetattr(STDIN_FILENO, &_termios);
        _termios.c_lflag &= ~(ICANON | ECHO | ISIG); // Disable canonical mode and echo
        tcsetattr(STDIN_FILENO, 0, &_termios);       // Set modified attributes

        // Prompt for username.
        do {
            puts("Username: ");
        } while (__read_input(username, CREDENTIALS_LENGTH, 1) <= 0);

        // Prompt for password (hidden input).
        printf("Password: ");
        if (__read_input(password, CREDENTIALS_LENGTH, 0) < 0) {
            fprintf(stderr, "Error reading password\n");
            return EXIT_FAILURE;
        }
        putchar('\n');

        // Restore terminal attributes
        tcgetattr(STDIN_FILENO, &_termios);
        _termios.c_lflag |= (ICANON | ECHO | ISIG); // Re-enable canonical mode and echo
        tcsetattr(STDIN_FILENO, 0, &_termios);

        // Retrieve user information based on the username
        if ((pwd = getpwnam(username)) == NULL) {
            if (errno == ENOENT) {
                printf("The given name was not found.\n");
            } else if (errno == 0) {
                printf("Cannot access passwd file.\n");
            } else {
                printf("Unknown error (%s).\n", strerror(errno));
            }
            continue; // Retry after error
        }

        struct spwd *spwd;
        if ((spwd = getspnam(username)) == NULL) {
            printf("Could not retrieve the secret password of %s: %s\n", username, strerror(errno));
            continue; // Retry if unable to get shadow password
        }

        // Hash the input password for verification
        unsigned char hash[SHA256_BLOCK_SIZE]       = { 0 };
        char hash_string[SHA256_BLOCK_SIZE * 2 + 1] = { 0 };
        SHA256_ctx_t ctx;
        sha256_init(&ctx);
        for (unsigned i = 0; i < 100000; ++i) {
            sha256_update(&ctx, (unsigned char *)password, strlen(password));
        }
        sha256_final(&ctx, hash);
        sha256_bytes_to_hex(hash, SHA256_BLOCK_SIZE, hash_string, SHA256_BLOCK_SIZE * 2 + 1);

        // Verify the password against the stored hash
        if (strcmp(spwd->sp_pwdp, hash_string) != 0) {
            printf("Wrong password.\n");
            continue; // Retry on incorrect password
        }

        break; // Successful authentication

    } while (true);

    // Check if a shell is set for the user
    if (pwd->pw_shell == NULL) {
        printf("login: There is no shell set for the user `%s`.\n", pwd->pw_name);
        return 1; // Exit with error if no shell
    }

    // Set the USER.
    if (setenv("USER", pwd->pw_name, 1) == -1) {
        printf("login: Failed to setup the environmental variable `USER`.\n");
        return 1;
    }

    // Set the SHELL.
    if (setenv("SHELL", pwd->pw_shell, 1) == -1) {
        printf("login: Failed to setup the environmental variable `SHELL`.\n");
        return 1;
    }

    // Set the HOME.
    if (setenv("HOME", pwd->pw_dir, 1) == -1) {
        printf("login: Failed to setup the environmental variable `HOME`.\n");
        return 1;
    }

    // Change the group ID
    if (setgid(pwd->pw_gid) < 0) {
        printf("login: Failed to change group id: %s\n", strerror(errno));
        return 1; // Exit with error on group ID change failure
    }

    // Change the user ID
    if (setuid(pwd->pw_uid) < 0) {
        printf("login: Failed to change user id: %s\n", strerror(errno));
        return 1; // Exit with error on user ID change failure
    }

    printf("\n");

    // Print /etc/motd if it exists
    __print_message_file("/etc/motd");

    // Welcome the user
    puts(BG_WHITE FG_BLACK);
    printf("\nWelcome " FG_RED "%s" FG_BLACK "...\n", pwd->pw_name);
    puts(BG_BLACK FG_WHITE_BRIGHT);

    // Execute the user's shell
    char *_argv[] = { pwd->pw_shell, (char *)NULL };
    if (execv(pwd->pw_shell, _argv) == -1) {
        printf("login: Failed to execute the shell.\n");
        printf("login: %s.\n", strerror(errno));
        return 1; // Exit with error on shell execution failure
    }

    return 0; // Normal exit
}
