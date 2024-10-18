/// @file shell.c
/// @brief Implement shell functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[SHELL ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include <sys/unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/stat.h>
#include <signal.h>
#include <io/debug.h>
#include <io/ansi_colors.h>
#include <sys/bitops.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <termios.h>
#include <limits.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <ring_buffer.h>

/// Maximum length of commands.
#define CMD_LEN 64
/// Maximum lenght of the history.
#define HISTORY_MAX 3

static inline void rb_history_entry_copy(char *dest, const char *src, unsigned size)
{
    strncpy(dest, src, size);
}

/// Initialize the two-dimensional ring buffer for integers.
DECLARE_FIXED_SIZE_2D_RING_BUFFER(char, history, HISTORY_MAX, CMD_LEN, 0)

// Required by `export`
#define ENV_NORM 1
#define ENV_BRAK 2
#define ENV_PROT 3

// History of commands.
static rb_history_t history;
// History reading index.
static unsigned history_index;
// Store the last command status
static int status = 0;
// Store the last command status as string
static char status_buf[4] = { 0 };

static sigset_t oldmask;

static void __block_sigchld(void)
{
    sigset_t mask;
    //sigmask functions only fail on invalid inputs -> no exception handling needed
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
}

static void __unblock_sigchld(void)
{
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

static inline int __is_separator(char c)
{
    return ((c == '\0') || (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r'));
}

/// @brief Count the number of words in a given sentence.
/// @param sentence Pointer to the input sentence.
/// @return The number of words in the sentence or -1 if input is invalid.
static inline int __count_words(const char *sentence)
{
    // Check if the input sentence is valid.
    if (sentence == NULL) {
        return -1; // Return -1 to indicate an error.
    }
    int result     = 0;
    bool_t inword  = false;
    const char *it = sentence;
    // Iterate over each character in the sentence.
    do {
        // Check if the current character is a word separator.
        if (__is_separator(*it)) {
            // If we are currently inside a word, mark the end of the word.
            if (inword) {
                inword = false;
                result++; // Increment the word count.
            }
        } else {
            // If the character is not a separator, mark that we are inside a word.
            inword = true;
        }
    } while (*it++);
    return result;
}

static inline int __folder_contains(
    const char *folder,
    const char *entry,
    int accepted_type,
    dirent_t *result)
{
    int fd = open(folder, O_RDONLY | O_DIRECTORY, 0);
    if (fd == -1) {
        return 0;
    }
    // Prepare the variables for the search.
    dirent_t dent;
    size_t entry_len;
    int found = 0;

    entry_len = strlen(entry);
    if (entry_len == 0) {
        return 0;
    }

    while (getdents(fd, &dent, sizeof(dirent_t)) == sizeof(dirent_t)) {
        if (accepted_type && (accepted_type != dent.d_type)) {
            continue;
        }
        if (strncmp(entry, dent.d_name, entry_len) == 0) {
            *result = dent;
            found   = 1;
            break;
        }
    }
    close(fd);
    return found;
}

static inline int __search_in_path(const char *entry, dirent_t *result)
{
    // Determine the search path.
    char *PATH_VAR = getenv("PATH");
    if (PATH_VAR == NULL)
        PATH_VAR = "/bin:/usr/bin";
    // Copy the path.
    char *path = strdup(PATH_VAR);
    // Iterate through the path entries.
    char *token = strtok(path, ":");
    if (token == NULL) {
        free(path);
        return 0;
    }
    do {
        if (__folder_contains(token, entry, DT_REG, result)) {
            return 1;
        }
    } while ((token = strtok(NULL, ":")));
    free(path);
    return 0;
}

/// @brief Prints the prompt.
static inline void __prompt_print(void)
{
    // Get the current working directory.
    char CWD[PATH_MAX];
    getcwd(CWD, PATH_MAX);
    // If the current working directory is equal to HOME, show ~.
    char *HOME = getenv("HOME");
    if (HOME != NULL)
        if (strcmp(CWD, HOME) == 0)
            strcpy(CWD, "~\0");
    // Get the user.
    char *USER = getenv("USER");
    if (USER == NULL) {
        USER = "error";
    }
    time_t rawtime = time(NULL);
    tm_t *timeinfo = localtime(&rawtime);
    // Get the hostname.
    char *HOSTNAME;
    utsname_t buffer;
    if (uname(&buffer) < 0) {
        HOSTNAME = "error";
    } else {
        HOSTNAME = buffer.nodename;
    }
    printf(FG_GREEN "%s" FG_WHITE "@" FG_CYAN "%s " FG_BLUE_BRIGHT "[%02d:%02d:%02d]" FG_WHITE " [%s] " FG_RESET "\n-> %% ",
           USER, HOSTNAME, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, CWD);
}

static char *__getenv(const char *var)
{
    if (strlen(var) > 1) {
        return getenv(var);
    }

    if (var[0] == '?') {
        sprintf(status_buf, "%d", status);
        return status_buf;
    }

    // TODO: implement access to argv
    /* int arg = strtol(var, NULL, 10); */
    /* if (arg < argc) { */
    /* return argv[arg]; */
    /* } */

    return NULL;
}

static void ___expand_env(char *str, char *buf, size_t buf_len, size_t str_len, bool_t null_terminate)
{
    // Buffer where we store the name of the variable.
    char buffer[BUFSIZ] = { 0 };
    // Flags used to keep track of the special characters.
    unsigned flags = 0;
    // We keep track of where the variable names starts
    char *env_start = NULL;
    // Where we store the retrieved environmental variable value.
    char *ENV = NULL;
    // Get the length of the string.
    if (!str_len) {
        str_len = strlen(str);
    }
    // Position where we are writing on the buffer.
    int b_pos = 0;
    // Iterate the string.
    for (int s_pos = 0; s_pos < str_len; ++s_pos) {
        if ((s_pos == 0) && str[s_pos] == '"')
            continue;
        if ((s_pos == (str_len - 1)) && str[s_pos] == '"')
            continue;
        // If we find the backslash, we need to protect the next character.
        if (str[s_pos] == '\\') {
            if (bit_check(flags, ENV_PROT))
                buf[b_pos++] = '\\';
            else
                bit_set_assign(flags, ENV_PROT);
            continue;
        }
        // If we find the dollar, we need to catch the meaning.
        if (str[s_pos] == '$') {
            // If the previous character is a backslash, we just need to print the dollar.
            if (bit_check(flags, ENV_PROT)) {
                buf[b_pos++] = '$';
            } else if ((s_pos < (str_len - 2)) && ((str[s_pos + 1] == '{'))) {
                // Toggle the open bracket method of accessing env variables.
                bit_set_assign(flags, ENV_BRAK);
                // We need to skip both the dollar and the open bracket `${`.
                env_start = &str[s_pos + 2];
            } else {
                // Toggle the normal method of accessing env variables.
                bit_set_assign(flags, ENV_NORM);
                // We need to skip the dollar `$`.
                env_start = &str[s_pos + 1];
            }
            continue;
        }
        if (bit_check(flags, ENV_BRAK)) {
            if (str[s_pos] == '}') {
                // Copy the environmental variable name.
                strncpy(buffer, env_start, &str[s_pos] - env_start);
                // Search for the environmental variable, and print it.
                if ((ENV = __getenv(buffer)))
                    for (int k = 0; k < strlen(ENV); ++k)
                        buf[b_pos++] = ENV[k];
                // Remove the flag.
                bit_clear_assign(flags, ENV_BRAK);
            }
            continue;
        }
        if (bit_check(flags, ENV_NORM)) {
            if (str[s_pos] == ':') {
                // Copy the environmental variable name.
                strncpy(buffer, env_start, &str[s_pos] - env_start);
                // Search for the environmental variable, and print it.
                if ((ENV = __getenv(buffer)))
                    for (int k = 0; k < strlen(ENV); ++k)
                        buf[b_pos++] = ENV[k];
                // Copy the `:`.
                buf[b_pos++] = str[s_pos];
                // Remove the flag.
                bit_clear_assign(flags, ENV_NORM);
            }
            continue;
        }
        buf[b_pos++] = str[s_pos];
    }
    if (bit_check(flags, ENV_NORM)) {
        // Copy the environmental variable name.
        size_t var_len = str_len - (env_start - str);
        strncpy(buffer, env_start, var_len);
        // Search for the environmental variable, and print it.
        if ((ENV = __getenv(buffer))) {
            for (int k = 0; k < strlen(ENV); ++k) {
                buf[b_pos++] = ENV[k];
            }
        }
    }

    if (null_terminate) {
        buf[b_pos] = 0;
    }
}

static void __expand_env(char *str, char *buf, size_t buf_len)
{
    ___expand_env(str, buf, buf_len, 0, false);
}

static int __export(int argc, char *argv[])
{
    char name[BUFSIZ] = { 0 }, value[BUFSIZ] = { 0 };
    char *first, *last;
    size_t name_len, value_start;
    for (int i = 1; i < argc; ++i) {
        // Get a pointer to the first and last occurrence of `=` inside the argument.
        first = strchr(argv[i], '='), last = strrchr(argv[i], '=');
        // Check validity of first and last, and check that they are the same.
        if (!first || !last || (last < argv[i]) || (first != last))
            continue;
        // Length of the name.
        name_len = last - argv[i];
        // Set where the value starts.
        value_start = (last + 1) - argv[i];
        // Copy the name.
        strncpy(name, argv[i], name_len);
        // Expand the environmental variables inside the argument.
        __expand_env(&argv[i][value_start], value, BUFSIZ);
        // Try to set the environmental variable.
        if ((strlen(name) > 0) && (strlen(value) > 0)) {
            if (setenv(name, value, 1) == -1) {
                printf("Failed to set environmental variable.\n");
                return 1;
            }
        }
    }
    return 0;
}

static int __cd(int argc, char *argv[])
{
    if (argc > 2) {
        printf("%s: too many arguments\n", argv[0]);
        return 1;
    }
    const char *path = NULL;
    if (argc == 2) {
        path = argv[1];
    } else {
        path = getenv("HOME");
        if (path == NULL) {
            printf("cd: There is no home directory set.\n");
            return 1;
        }
    }
    // Get the real path.
    char real_path[PATH_MAX];
    if (realpath(path, real_path, PATH_MAX) != real_path) {
        printf("cd: Failed to resolve directory.\n");
        return 1;
    }
    // Stat the directory.
    stat_t dstat;
    if (stat(real_path, &dstat) == -1) {
        printf("cd: cannot stat '%s': %s\n", real_path, strerror(errno));
        return 1;
    }
    // Check if the directory is actually a symbolic link.
    if (S_ISLNK(dstat.st_mode)) {
        char link_buffer[PATH_MAX];
        ssize_t len;
        // First, read the link.
        if ((len = readlink(real_path, link_buffer, sizeof(link_buffer))) < 0) {
            printf("cd: Failed to read symlink.\n");
            return 1;
        }
        link_buffer[len] = '\0';
        // Resolve the link, it might still be a relative path.
        if (realpath(link_buffer, real_path, PATH_MAX) != real_path) {
            printf("cd: Failed to resolve symlink to directory.\n");
            return 1;
        }
    }
    // Open the given directory.
    int fd = open(real_path, O_RDONLY | O_DIRECTORY, S_IXUSR);
    if (fd == -1) {
        printf("cd: %s: %s\n", real_path, strerror(errno));
        return 1;
    }
    // Set current working directory.
    chdir(real_path);
    close(fd);
    // Get the updated working directory.
    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    // Update the environmental variable.
    if (setenv("PWD", cwd, 1) == -1) {
        printf("cd: Failed to set current working directory.\n");
        return 1;
    }
    putchar('\n');
    return 0;
}

void __history_print(void)
{
    rb_history_entry_t entry;
    rb_history_init_entry(&entry);
    pr_notice("H[S:%2u, C:%2u] :\n", history.size, history.count);
    for (unsigned i = 0; i < history.count; i++) {
        rb_history_get(&history, i, &entry);
        pr_notice("[%2u] %s\n", i, entry.buffer);
    }
    pr_notice("\n");
}

/// @brief Push the command inside the history.
/// @param entry The history entry to be added.
static inline void __history_push(rb_history_entry_t *entry)
{
    // Push the new entry to the back of the history ring buffer.
    rb_history_push_back(&history, entry);
    // Set the history index to the current count, pointing to the end.
    history_index = history.count;
}

/// @brief Give the key allows to navigate through the history.
static rb_history_entry_t *__history_fetch(char direction)
{
    // If history is empty, return NULL.
    if (history.count == 0) {
        return NULL;
    }
    // Move to the previous entry if direction is UP and index is greater than 0.
    if ((direction == 'A') && (history_index > 0)) {
        history_index--;
    }
    // Move to the next entry if direction is DOWN and index is less than the history count.
    else if ((direction == 'B') && (history_index < history.count)) {
        history_index++;
    }
    // Check if we reached the end of the history in DOWN direction.
    if ((direction == 'B') && (history_index == history.count)) {
        return NULL;
    }
    // Return the current history entry, adjusting for buffer wrap-around.
    return history.buffer + ((history.tail + history_index) % history.size);
}

/// @brief Append a character to the history entry buffer.
/// @param entry Pointer to the history entry structure.
/// @param index Pointer to the current index in the buffer.
/// @param length Pointer to the current length of the buffer.
/// @param c Character to append to the buffer.
/// @return Returns 1 if the buffer limit is reached, 0 otherwise.
static inline int __command_append(
    rb_history_entry_t *entry,
    int *index,
    int *length,
    char c)
{
    // Insert the new character at the current index in the buffer, then
    // increment the index.
    entry->buffer[(*index)++] = c;
    // Increase the length of the buffer to reflect the added character.
    (*length)++;
    // Display the newly added character to the standard output.
    putchar(c);
    // Check if the buffer limit has been reached.
    if ((*index) == (entry->size - 1)) {
        // Null-terminate the buffer to ensure it is a valid string.
        entry->buffer[(*index)] = 0;
        // Indicate that the buffer is full.
        return 1;
    }
    // The buffer limit has not been reached.
    return 0;
}

/// @brief Suggests a directory entry to be appended to the current command
/// buffer.
/// @param filename Pointer to the file name.
/// @param filetype The file type.
/// @param entry Pointer to the history entry structure.
/// @param index Pointer to the current index in the buffer.
/// @param length Pointer to the current length of the buffer.
static inline void __command_suggest(
    const char *filename,
    int filetype,
    int offset,
    rb_history_entry_t *entry,
    int *index,
    int *length)
{
    // Check if there is a valid suggestion to process.
    if (filename) {
        // Iterate through the characters of the suggested directory entry name.
        for (int i = offset; i < strlen(filename); ++i) {
            pr_crit("[%2d] '%c'\n",
                    i,
                    filename[i],
                    entry->buffer);
            // Append the current character to the buffer.
            // If the buffer is full, break the loop.
            if (__command_append(entry, index, length, filename[i])) {
                break;
            }
        }
        // If the suggestion is a directory, append a trailing slash.
        if ((filetype == DT_DIR) && (entry->buffer[(*index) - 1] != '/')) {
            // Append the slash character to indicate a directory.
            __command_append(entry, index, length, '/');
        }
    }
}

/// @brief Completes the current command based on the given entry, index, and length.
/// @param entry The history entry containing the command buffer.
/// @param index The current index in the command buffer.
/// @param length The total length of the command buffer.
/// @return Returns 0 on success, 1 on failure.
static int __command_complete(
    rb_history_entry_t *entry,
    int *index,
    int *length)
{
    pr_crit("__command_complete(%s, %2d, %2d)\n", entry->buffer, *index, *length);

    char cwd[PATH_MAX]; // Buffer to store the current working directory.
    int words;          // Variable to store the word count.
    dirent_t dent;      // Prepare the dirent variable.

    // Count the number of words in the command buffer.
    words = __count_words(entry->buffer);

    // If there are no words in the command buffer, log it and return.
    if (words == 0) {
        pr_crit("__command_complete(%s, %2d, %2d) : No words.\n", entry->buffer, *index, *length);
        return 0;
    }

    // Determines if we are at the beginning of a new argument, last character is space.
    if (__is_separator(entry->buffer[(*index) - 1])) {
        pr_crit("__command_complete(%s, %2d, %2d) : Separator.\n", entry->buffer, *index, *length);
        return 0;
    }

    // If the last two characters are two dots `..` append a slash `/`, and
    // continue.
    if (((*index) >= 2) && ((entry->buffer[(*index) - 1] == '.') && (entry->buffer[(*index) - 2] == '.'))) {
        pr_crit("__command_complete(%s, %2d, %2d) : Append '/'.\n", entry->buffer, *index, *length);
        if (__command_append(entry, index, length, '/')) {
            pr_crit("Failed to append character.\n");
            return 1;
        }
    }

    // Attempt to retrieve the current working directory.
    if (getcwd(cwd, PATH_MAX) == NULL) {
        // Error handling: If getcwd fails, it returns NULL
        pr_crit("Failed to get current working directory.\n");
        return 1;
    }

    // Determines if we are executing a command from current directory.
    int is_run_cmd = ((*index) >= 2) && (entry->buffer[0] == '.') && (entry->buffer[1] == '/');
    // Determines if we are entering an absolute path.
    int is_abs_path = ((*index) >= 1) && (entry->buffer[0] == '/');

    // If there is only one word, we are searching for a command.
    if (is_run_cmd) {
        if (__folder_contains(cwd, entry->buffer + 2, 0, &dent)) {
            pr_crit("__command_complete(%s, %2d, %2d) : Suggest '%s' -> '%s'.\n", entry->buffer, *index, *length,
                    entry->buffer + 2, dent.d_name);
            __command_suggest(
                dent.d_name,
                dent.d_type,
                (*index) - 2,
                entry,
                index,
                length);
        }
    } else if (is_abs_path) {
        char _dirname[PATH_MAX];
        if (!dirname(entry->buffer, _dirname, sizeof(_dirname))) {
            return 0;
        }
        const char *_basename = basename(entry->buffer);
        if (!_basename) {
            return 0;
        }
        if ((*_dirname == 0) || (*_basename == 0)) {
            return 0;
        }
        if (__folder_contains(_dirname, _basename, 0, &dent)) {
            pr_crit("__command_complete(%s, %2d, %2d) : Suggest abs '%s' -> '%s'.\n", entry->buffer, *index, *length,
                    entry->buffer, dent.d_name);
            __command_suggest(
                dent.d_name,
                dent.d_type,
                strlen(_basename),
                entry,
                index,
                length);
        }
    } else if (words == 1) {
        if (__search_in_path(entry->buffer, &dent)) {
            pr_crit("__command_complete(%s, %2d, %2d) : Suggest in path '%s' -> '%s'.\n", entry->buffer, *index, *length,
                    entry->buffer, dent.d_name);
            __command_suggest(
                dent.d_name,
                dent.d_type,
                *index,
                entry,
                index,
                length);
        }
    } else {
        // Search the last occurrence of a space, from there on
        // we will have the last argument.
        char *last_argument = strrchr(entry->buffer, ' ');
        // We need to move ahead of one character if we found the space.
        last_argument = last_argument ? last_argument + 1 : NULL;
        // If there is no last argument.
        if (last_argument == NULL) {
            return 0;
        }
        char _dirname[PATH_MAX];
        if (!dirname(last_argument, _dirname, sizeof(_dirname))) {
            return 0;
        }
        const char *_basename = basename(last_argument);
        if (!_basename) {
            return 0;
        }
        if ((*_dirname != 0) && (*_basename != 0)) {
            if (__folder_contains(_dirname, _basename, 0, &dent)) {
                pr_crit("__command_complete(%s, %2d, %2d) : Suggest '%s' -> '%s'.\n", entry->buffer, *index, *length,
                        last_argument, dent.d_name);
                //__command_suggest(&dent, strlen(_basename));
            }
        } else if (*_basename != 0) {
            if (__folder_contains(cwd, _basename, 0, &dent)) {
                pr_crit("__command_complete(%s, %2d, %2d) : Suggest '%s' -> '%s'.\n", entry->buffer, *index, *length,
                        last_argument, dent.d_name);
                //__command_suggest(&dent, strlen(_basename));
            }
        }
    }
    return 0;
}

/// @brief Reads user input into a buffer, supporting basic editing features.
/// @param buffer The buffer to store the input string.
/// @param bufsize The maximum size of the buffer.
/// @return The length of the input read, or -1 if a special command (Ctrl+C) is
/// detected.
static inline int __read_command(rb_history_entry_t *entry)
{
    int index = 0, c, length = 0, insert_active = 0;

    // Clear the buffer at the start
    memset(entry->buffer, 0, entry->size);

    do {
        c = getchar(); // Read a character from input

        //pr_debug("[%2d      ] %c (%u) (0)\n", index, c, c);

        // Ignore EOF and null or tab characters
        if (c == EOF || c == 0) {
            continue;
        }

        // Handle newline character to finish input
        if (c == '\n') {
            putchar('\n'); // Display a newline
            return length; // Return length of input
        }

        // Handle delete character.
        if (c == 127) {
            if (index < length) {
                --length;     // Decrease length
                putchar(127); // Show delete character.
                // Shift left to remove character at index
                memmove(entry->buffer + index, entry->buffer + index + 1, length - index + 1);
            }
            continue;
        }

        // Handle backspace for deletion
        if (c == '\b') {
            if (index > 0) {
                --length; // Decrease length
                --index;  // Move index back
                // Shift the buffer left to remove the character
                memmove(entry->buffer + index, entry->buffer + index + 1, length - index + 1);
                // Show backspace action.
                putchar('\b');
            }
            continue;
        }

        if (c == '\t') {
            __command_complete(entry, &index, &length);
            continue;
        }

        // Handle space character
        if (c == ' ') {
            // Shift buffer to the right to insert space
            memmove(entry->buffer + index + 1, entry->buffer + index, length - index + 1);
            entry->buffer[index++] = c; // Insert space
            length++;
            // Show space.
            putchar(c);
            continue;
        }

        // Handle escape sequences (for arrow keys, home, end, etc.)
        if (c == '\033') {
            c = getchar(); // Get the next character
            //pr_debug("[%2d      ] %c (%u) (1)\n", index, c, c);
            if (c == '[') {
                // Get the direction key (Left, Right, Home, End, Insert, Delete)
                c = getchar();
                //pr_debug("[%2d      ] %c (%u) (2)\n", index, c, c);
                if ((c == 'A') || (c == 'B')) {
                    // Clear the current command.
                    memset(entry->buffer, 0, entry->size);
                    // Clear the current command from display
                    while (index--) { putchar('\b'); }
                    // Reset index.
                    index = 0;
                    // Fetch the history element.
                    rb_history_entry_t *history_entry = __history_fetch(c);
                    if (history_entry != NULL) {
                        // Sets the command.
                        rb_history_entry_copy(entry->buffer, history_entry->buffer, entry->size);
                        // Print the old command.
                        printf(entry->buffer);
                        // Set index to the end.
                        index = strnlen(entry->buffer, entry->size);
                    }
                }
                // LEFT Arrow.
                else if (c == 'D') {
                    pr_debug("%d > 0\n", index);
                    if (index > 0) {
                        puts("\033[1D"); // Move the cursor left
                        index--;         // Decrease index
                    }
                }
                // RIGHT Arrow.
                else if (c == 'C') {
                    pr_debug("%d < %d\n", index, length);
                    if (index < length) {
                        puts("\033[1C"); // Move the cursor right
                        index++;         // Increase index
                    }
                }
                // HOME
                else if (c == '1') {
                    if (getchar() == '~') {
                        printf("\033[%dD", index); // Move cursor to the beginning
                        index = 0;                 // Set index to the start
                    }
                }
                // END
                else if (c == '4') {
                    if (getchar() == '~') {
                        printf("\033[%dC", length - index); // Move cursor to the end
                        index = length;                     // Set index to the end
                    }
                }
                // INSERT
                else if (c == '2') {
                    if (getchar() == '~') {
                        insert_active = !insert_active; // Toggle insert mode
                    }
                } else if ((c == '5') && (getchar() == '~')) { // PAGE_UP
                    // Nothing to do.
                } else if ((c == '6') && (getchar() == '~')) { // PAGE_DOWN
                    // Nothing to do.
                }

            } else if (c == '^') {
                // Handle special commands (Ctrl+C, Ctrl+U)
                c = getchar();
                //pr_debug("[%2d      ] %c (%u) (2)\n", index, c, c);
                if (c == 'C') {
                    memset(entry->buffer, 0, entry->size); // Clear buffer
                    putchar('\n');
                    return -1; // Return -1 on Ctrl+C
                }

                if (c == 'U') {
                    // Clear the current command.
                    memset(entry->buffer, 0, entry->size);
                    // Clear the current command from display
                    while (index--) {
                        putchar('\b'); // Move cursor back.
                    }
                    index  = 0; // Reset index.
                    length = 0;
                }
            }
            continue;
        }

        // Handle insertion based on insert mode
        if (!insert_active) {
            // Shift buffer to the right to insert new character
            memmove(entry->buffer + index + 1, entry->buffer + index, length - index + 1);
        } else if (index < length - 1) {
            puts("\033[1C"); // Move cursor right
            putchar('\b');   // Prepare to delete the character
        }

        //pr_debug("[%2d -> %2u] %c (%u) (0)\n", index, index + 1, c, c);

        // Append the character.
        if (__command_append(entry, &index, &length, c)) {
            break; // Exit loop if buffer is full.
        }

    } while (length < entry->size);

    return length; // Return total length of input
}

/// @brief Gets the options from the command.
/// @param command The executed command.
static void __alloc_argv(char *command, int *argc, char ***argv)
{
    (*argc) = __count_words(command);
    // Get the number of arguments, return if zero.
    if ((*argc) == 0) {
        return;
    }
    (*argv)        = (char **)malloc(sizeof(char *) * ((*argc) + 1));
    bool_t inword  = false;
    char *cit      = command;
    char *argStart = command;
    size_t argcIt  = 0;
    do {
        if (!__is_separator(*cit)) {
            if (!inword) {
                argStart = cit;
                inword   = true;
            }
            continue;
        }

        if (inword) {
            inword = false;
            // Expand possible environment variables in the current argument
            char expand_env_buf[BUFSIZ];
            ___expand_env(argStart, expand_env_buf, BUFSIZ, cit - argStart, true);
            (*argv)[argcIt] = (char *)malloc(strlen(expand_env_buf) + 1);
            strcpy((*argv)[argcIt++], expand_env_buf);
        }
    } while (*cit++);
    (*argv)[argcIt] = NULL;
}

static inline void __free_argv(int argc, char **argv)
{
    for (int i = 0; i < argc; ++i) {
        free(argv[i]);
    }
    free(argv);
}

static void __setup_redirects(int *argcp, char ***argvp)
{
    char **argv = *argvp;
    int argc    = *argcp;

    char *path;
    int flags   = O_CREAT | O_WRONLY;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

    bool_t rd_stdout, rd_stderr;
    rd_stdout = rd_stderr = 0;

    for (int i = 1; i < argc - 1; ++i) {
        if (!strstr(argv[i], ">")) {
            continue;
        }

        path = argv[i + 1];

        // Determine stream to redirect
        switch (*argv[i]) {
        case '&':
            rd_stdout = rd_stderr = true;
            break;
        case '2':
            rd_stderr = true;
            break;
        case '>':
            rd_stdout = true;
            break;
        default:
            continue;
        }

        // Determine open flags
        if (strstr(argv[i], ">>")) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }

        // Remove redirects from argv
        *argcp -= 2;
        free(argv[i]);
        (*argvp)[i] = 0;
        free(argv[i + 1]);
        (*argvp)[i + 1] = 0;

        int fd = open(path, flags, mode);
        if (fd < 0) {
            printf("%s: Failed to open file\n", path);
            exit(1);
        }

        if (rd_stdout) {
            close(STDOUT_FILENO);
            dup(fd);
        }

        if (rd_stderr) {
            close(STDERR_FILENO);
            dup(fd);
        }
        close(fd);
        break;
    }
}

static int __execute_command(rb_history_entry_t *entry)
{
    int _status = 0;
    // Retrieve the options from the command.
    // The current number of arguments.
    int _argc = 1;
    // The vector of arguments.
    char **_argv;
    __alloc_argv(entry->buffer, &_argc, &_argv);
    // Check if the command is empty.
    if (_argc == 0) {
        return 0;
    }

    if (!strcmp(_argv[0], "init")) {
    } else if (!strcmp(_argv[0], "cd")) {
        __cd(_argc, _argv);
    } else if (!strcmp(_argv[0], "..")) {
        const char *__argv[] = { "cd", "..", NULL };
        __cd(2, (char **)__argv);
    } else if (!strcmp(_argv[0], "export")) {
        __export(_argc, _argv);
    } else {
        bool_t blocking = true;
        if (strcmp(_argv[_argc - 1], "&") == 0) {
            blocking = false;
            _argc--;
            free(_argv[_argc]);
            _argv[_argc] = NULL;
        }

        __block_sigchld();

        // Is a shell path, execute it!
        pid_t cpid = fork();
        if (cpid == 0) {
            // Makes the new process a group leader
            pid_t pid = getpid();
            setpgid(cpid, pid);

            __unblock_sigchld();

            __setup_redirects(&_argc, &_argv);

            if (execvp(_argv[0], _argv) == -1) {
                printf("\nUnknown command: %s\n", _argv[0]);
                exit(127);
            }
        }
        if (blocking) {
            waitpid(cpid, &_status, 0);
            if (WIFSIGNALED(_status)) {
                printf(FG_RED "\nExit status %d, killed by signal %d\n" FG_RESET,
                       WEXITSTATUS(_status), WTERMSIG(_status));
            } else if (WIFSTOPPED(_status)) {
                printf(FG_YELLOW "\nExit status %d, stopped by signal %d\n" FG_RESET,
                       WEXITSTATUS(_status), WSTOPSIG(_status));
            } else if (WEXITSTATUS(_status) != 0) {
                printf(FG_RED "\nExit status %d\n" FG_RESET, WEXITSTATUS(_status));
            }
        }
        __unblock_sigchld();
    }
    // Free up the memory reserved for the arguments.
    __free_argv(_argc, _argv);
    status = WEXITSTATUS(_status);
    return status;
}

static int __execute_file(char *path)
{
    rb_history_entry_t entry;
    rb_history_init_entry(&entry);
    int fd;
    if ((fd = open(path, O_RDONLY, 0)) == -1) {
        printf("%s: %s\n", path, strerror(errno));
        return -errno;
    }
    while (fgets(entry.buffer, entry.size, fd)) {
        if (entry.buffer[0] == '#') {
            continue;
        }

        if ((status = __execute_command(&entry)) != 0) {
            printf("\n%s: exited with %d\n", entry.buffer, status);
        }
    }

    close(fd);
    return status;
}

static void __interactive_mode(void)
{
    rb_history_entry_t entry;
    rb_history_init_entry(&entry);

    stat_t buf;
    if (stat(".shellrc", &buf) == 0) {
        int ret = __execute_file(".shellrc");
        if (ret < 0) {
            printf("%s: .shellrc: %s\n", strerror(-ret));
        }
    }
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while (true) {
        // First print the prompt.
        __prompt_print();

        // Get terminal attributes for input handling
        struct termios _termios;
        tcgetattr(STDIN_FILENO, &_termios);
        _termios.c_lflag &= ~(ICANON | ECHO | ISIG); // Disable canonical mode and echo
        tcsetattr(STDIN_FILENO, 0, &_termios);       // Set modified attributes

        // Get the input command.
        if (__read_command(&entry) < 0) {
            pr_crit("Error reading command...\n");
            // Restore terminal attributes
            tcgetattr(STDIN_FILENO, &_termios);
            _termios.c_lflag |= (ICANON | ECHO | ISIG); // Re-enable canonical mode and echo
            tcsetattr(STDIN_FILENO, 0, &_termios);
            continue;
        }

        // Restore terminal attributes
        tcgetattr(STDIN_FILENO, &_termios);
        _termios.c_lflag |= (ICANON | ECHO | ISIG); // Re-enable canonical mode and echo
        tcsetattr(STDIN_FILENO, 0, &_termios);

        // Add the command to the history.
        if (strnlen(entry.buffer, entry.size) > 0) {
            __history_push(&entry);
        }

        // Execute the command.
        __execute_command(&entry);
    }
#pragma clang diagnostic pop
}

void wait_for_child(int signum)
{
    wait(NULL);
}

int main(int argc, char *argv[])
{
    setsid();
    // Initialize the history.
    rb_history_init(&history, rb_history_entry_copy);

    char *USER = getenv("USER");
    if (USER == NULL) {
        printf("shell: There is no user set.\n");
        return 1;
    }
    if (getenv("PATH") == NULL) {
        if (setenv("PATH", "/bin:/usr/bin", 0) == -1) {
            printf("shell: Failed to set PATH.\n");
            return 1;
        }
    }
    // Set the signal handler to handle the termination of the child.
    sigaction_t action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = wait_for_child;
    if (sigaction(SIGCHLD, &action, NULL) == -1) {
        printf("Failed to set signal handler (%s).\n", SIGCHLD, strerror(errno));
        return 1;
    }

    // We have been executed as script interpreter
    if (!strstr(argv[0], "shell")) {
        return __execute_file(argv[1]);
    }

    // Interactive
    if (argc == 1) {
        // Move inside the home directory.
        __cd(0, NULL);
        __interactive_mode();
    } else {
        // check file arguments
        for (int i = 1; i < argc; ++i) {
            stat_t buf;
            if (stat(argv[i], &buf) < 0) {
                printf("%s: No such file\n", argv[i]);
                exit(1);
            }
        }

        for (int i = 1; i < argc; ++i) {
            if (!(status = __execute_file(argv[i]))) {
                return status;
            }
        }
    }

    return 0;
}
