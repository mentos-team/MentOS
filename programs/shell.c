/// @file shell.c
/// @brief Implement shell functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[SHELL ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

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
#define HISTORY_MAX 10

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
        pr_crit("__count_words: Invalid input, sentence is NULL.\n");
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

/// @brief Checks if a specified folder contains a specific entry of a certain type.
/// @param folder The path to the folder to search in.
/// @param entry The name of the entry to search for.
/// @param accepted_type The type of entry to search for (e.g., file, directory).
/// @param result Pointer to store the found entry, if any.
/// @return Returns 1 if the entry is found, 0 otherwise or in case of an error.
static inline int __folder_contains(
    const char *folder,
    const char *entry,
    int accepted_type,
    dirent_t *result)
{
    // Validate input parameters.
    if ((folder == NULL) || (entry == NULL) || (result == NULL)) {
        pr_crit("__folder_contains: Invalid input parameters.\n");
        return 0; // Return 0 to indicate an error.
    }
    // Attempt to open the folder with read-only and directory flags.
    int fd = open(folder, O_RDONLY | O_DIRECTORY, 0);
    if (fd == -1) {
        pr_crit("__folder_contains: Failed to open folder: %s\n", folder);
        return 0; // Return 0 if the folder couldn't be opened.
    }
    // Prepare variables for the search.
    dirent_t dent;    // Variable to hold the directory entry during iteration.
    size_t entry_len; // Length of the entry name.
    int found = 0;    // Flag to indicate if the entry was found.
    // Calculate the length of the entry name.
    entry_len = strlen(entry);
    if (entry_len == 0) {
        pr_crit("__folder_contains: Invalid entry name (empty).\n");
        close(fd); // Close the folder before returning.
        return 0;  // Return 0 if the entry name is empty.
    }
    // Iterate over the directory entries.
    while (getdents(fd, &dent, sizeof(dirent_t)) == sizeof(dirent_t)) {
        // If an accepted type is specified and doesn't match the current entry type, skip.
        if (accepted_type && (accepted_type != dent.d_type)) {
            continue;
        }
        // Compare the entry name with the current directory entry name.
        if (strncmp(entry, dent.d_name, entry_len) == 0) {
            // If a match is found, store the result and mark the entry as found.
            *result = dent;
            found   = 1;
            break; // Exit the loop as the entry was found.
        }
    }
    // Close the directory file descriptor.
    close(fd);
    // Return whether the entry was found.
    return found;
}

/// @brief Searches for a specified entry in the system's PATH directories.
/// @param entry The name of the entry to search for.
/// @param result Pointer to store the found entry, if any.
/// @return Returns 1 if the entry is found, 0 otherwise.
static inline int __search_in_path(const char *entry, dirent_t *result)
{
    // Validate input parameters.
    if ((entry == NULL) || (result == NULL)) {
        pr_crit("__search_in_path: Invalid input parameters.\n");
        return 0; // Return 0 to indicate an error.
    }
    // Retrieve the PATH environment variable.
    char *PATH_VAR = getenv("PATH");
    if (PATH_VAR == NULL) {
        // If PATH is not set, default to commonly used binary directories.
        PATH_VAR = "/bin:/usr/bin";
    }
    // Prepare for tokenizing the path using custom logic.
    char token[NAME_MAX] = { 0 }; // Buffer to hold each token (directory).
    size_t offset        = 0;     // Offset for the tokenizer.
    // Iterate through each directory in the PATH.
    while (tokenize(PATH_VAR, ":", &offset, token, NAME_MAX)) {
        // Search for the entry in the current directory (tokenized directory).
        if (__folder_contains(token, entry, DT_REG, result)) {
            return 1; // Return 1 if the entry is found.
        }
    }
    return 0; // Return 0 if the entry was not found.
}

/// @brief Prints the command prompt with user, hostname, time, and current
/// working directory.
static inline void __prompt_print(void)
{
    // Get the current working directory.
    char CWD[PATH_MAX];
    if (getcwd(CWD, PATH_MAX) == NULL) {
        pr_crit("__prompt_print: Failed to get current working directory.\n");
        strcpy(CWD, "error");
    }
    // Get the HOME environment variable.
    char *HOME = getenv("HOME");
    if (HOME != NULL) {
        // If the current working directory is equal to HOME, replace it with '~'.
        if (strcmp(CWD, HOME) == 0) {
            strcpy(CWD, "~\0");
        }
    }
    // Get the USER environment variable.
    char *USER = getenv("USER");
    if (USER == NULL) {
        pr_crit("__prompt_print: Failed to get USER environment variable.\n");
        USER = "error";
    }
    // Get the current time.
    time_t rawtime = time(NULL);
    if (rawtime == (time_t)(-1)) {
        pr_crit("__prompt_print: Failed to get current time.\n");
        rawtime = 0; // Set to 0 in case of failure.
    }
    // Convert time to local time format.
    tm_t *timeinfo = localtime(&rawtime);
    if (timeinfo == NULL) {
        pr_crit("__prompt_print: Failed to convert time to local time.\n");
        timeinfo = &(tm_t){ 0 }; // Initialize to 0 to avoid segmentation faults.
    }
    // Get the hostname using uname.
    char *HOSTNAME;
    utsname_t buffer;
    if (uname(&buffer) < 0) {
        pr_crit("__prompt_print: Failed to get hostname using uname.\n");
        HOSTNAME = "error";
    } else {
        HOSTNAME = buffer.nodename;
    }
    // Print the formatted prompt.
    printf(FG_GREEN "%s" FG_WHITE "@" FG_CYAN "%s " FG_BLUE_BRIGHT "[%02d:%02d:%02d]" FG_WHITE " [%s] " FG_RESET "\n-> %% ",
           USER, HOSTNAME, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, CWD);
}

/// @brief Retrieves the value of the specified environment variable or special shell variables.
/// @param var The name of the environment variable or special variable to retrieve.
/// @return Returns the value of the variable if found, or NULL if not.
static char *__getenv(const char *var)
{
    // Ensure the input variable name is valid (non-NULL and non-empty).
    if (var == NULL || strlen(var) == 0) {
        return NULL;
    }
    // If the variable has a length greater than 1, retrieve it as a standard environment variable.
    if (strlen(var) > 1) {
        return getenv(var);
    }
    // Handle special variables like `$?`, which represents the status of the last command.
    if (var[0] == '?') {
        // Assuming 'status' is a global or accessible variable containing the last command status.
        sprintf(status_buf, "%d", status); // Convert the status to a string.
        return status_buf;                 // Return the status as a string.
    }
    // TODO: Implement access to `argv` for positional parameters (e.g., $1, $2).
#if 0
    int arg = strtol(var, NULL, 10);  // Convert the variable name to an integer.
    if (arg < argc) {  // Ensure the argument index is within bounds.
        return argv[arg];  // Return the corresponding argument from `argv`.
    }
#endif
    // If no match is found, return NULL.
    return NULL;
}

/// @brief Expands environmental variables in a string and stores the result in the buffer.
/// @param str The input string containing potential environmental variables.
/// @param buf The buffer where the expanded string will be stored.
/// @param buf_len The maximum length of the buffer.
/// @param str_len The length of the input string (if provided, otherwise it will be calculated).
/// @param null_terminate If true, the resulting buffer will be null-terminated.
static void ___expand_env(char *str, char *buf, size_t buf_len, size_t str_len, bool_t null_terminate)
{
    // Buffer where we store the name of the variable.
    char buffer[BUFSIZ] = { 0 };
    // Flags used to keep track of the special characters.
    unsigned flags = 0;
    // Pointer to where the variable name starts.
    char *env_start = NULL;
    // Pointer to store the retrieved environmental variable value.
    char *ENV = NULL;
    // If str_len is 0, calculate the length of the input string.
    if (!str_len) {
        str_len = strlen(str);
    }
    // Position where we are writing on the buffer.
    int b_pos = 0;
    // Iterate through the input string.
    for (int s_pos = 0; s_pos < str_len; ++s_pos) {
        // Skip quotes at the start and end of the string.
        if ((s_pos == 0 && str[s_pos] == '"') || (s_pos == (str_len - 1) && str[s_pos] == '"')) {
            continue;
        }
        // Handle backslash as escape character.
        if (str[s_pos] == '\\') {
            if (bit_check(flags, ENV_PROT)) {
                if (b_pos < buf_len - 1) {
                    buf[b_pos++] = '\\'; // If protected, add the backslash.
                }
            } else {
                bit_set_assign(flags, ENV_PROT); // Set the protection flag.
            }
            continue;
        }
        // Handle environmental variable expansion with $.
        if (str[s_pos] == '$') {
            if (bit_check(flags, ENV_PROT)) {
                // If protected by backslash, just add the dollar sign.
                if (b_pos < buf_len - 1) {
                    buf[b_pos++] = '$';
                }
            } else if ((s_pos < (str_len - 2)) && (str[s_pos + 1] == '{')) {
                // Handle ${VAR} syntax for environmental variables.
                bit_set_assign(flags, ENV_BRAK);
                env_start = &str[s_pos + 2]; // Start of the variable name.
            } else {
                // Handle normal $VAR syntax.
                bit_set_assign(flags, ENV_NORM);
                env_start = &str[s_pos + 1]; // Start of the variable name.
            }
            continue;
        }
        // Handle ${VAR} style environmental variables.
        if (bit_check(flags, ENV_BRAK)) {
            if (str[s_pos] == '}') {
                // Extract and expand the environmental variable name.
                strncpy(buffer, env_start, &str[s_pos] - env_start);
                buffer[&str[s_pos] - env_start] = '\0'; // Null-terminate.
                // Retrieve the value of the environmental variable.
                if ((ENV = __getenv(buffer)) != NULL) {
                    // Copy the value into the buffer.
                    for (int k = 0; k < strlen(ENV) && b_pos < buf_len - 1; ++k) {
                        buf[b_pos++] = ENV[k];
                    }
                }
                bit_clear_assign(flags, ENV_BRAK); // Clear the flag.
            }
            continue;
        }
        // Handle $VAR style environmental variables.
        if (bit_check(flags, ENV_NORM)) {
            if (str[s_pos] == ':') {
                strncpy(buffer, env_start, &str[s_pos] - env_start);
                buffer[&str[s_pos] - env_start] = '\0'; // Null-terminate.
                // Retrieve the value of the environmental variable.
                if ((ENV = __getenv(buffer)) != NULL) {
                    for (int k = 0; k < strlen(ENV) && b_pos < buf_len - 1; ++k) {
                        buf[b_pos++] = ENV[k];
                    }
                }
                // Add the ':' character and clear the flag.
                if (b_pos < buf_len - 1) {
                    buf[b_pos++] = str[s_pos];
                }
                bit_clear_assign(flags, ENV_NORM);
            }
            continue;
        }
        // Add normal characters to the buffer.
        if (b_pos < buf_len - 1) {
            buf[b_pos++] = str[s_pos];
        }
    }
    // Handle any remaining environmental variable in $VAR style.
    if (bit_check(flags, ENV_NORM)) {
        strncpy(buffer, env_start, str_len - (env_start - str));
        buffer[str_len - (env_start - str)] = '\0';
        if ((ENV = __getenv(buffer)) != NULL) {
            for (int k = 0; k < strlen(ENV) && b_pos < buf_len - 1; ++k) {
                buf[b_pos++] = ENV[k];
            }
        }
    }
    // Null-terminate the buffer if requested.
    if (null_terminate && b_pos < buf_len) {
        buf[b_pos] = '\0';
    }
}

/// @brief Simplified version of ___expand_env for use without specifying length and null-termination.
/// @param str The input string containing environmental variables.
/// @param buf The buffer where the expanded result will be stored.
/// @param buf_len The size of the buffer.
static void __expand_env(char *str, char *buf, size_t buf_len)
{
    ___expand_env(str, buf, buf_len, 0, false);
}

/// @brief Sets environment variables based on arguments.
/// @param argc The number of arguments passed.
/// @param argv The array of arguments, where each argument is a name-value pair (e.g., NAME=value).
/// @return Returns 0 on success, 1 on failure.
static int __export(int argc, char *argv[])
{
    char name[BUFSIZ] = { 0 }, value[BUFSIZ] = { 0 };
    char *first, *last;
    size_t name_len, value_start;
    // Loop through each argument, starting from argv[1].
    for (int i = 1; i < argc; ++i) {
        // Get a pointer to the first and last occurrence of `=` inside the argument.
        first = strchr(argv[i], '=');
        last  = strrchr(argv[i], '=');
        // Check the validity of first and last, and ensure they are the same (i.e., a single `=`).
        if (!first || !last || (last < argv[i]) || (first != last)) {
            printf("Invalid format: '%s'. Expected NAME=value format.\n", argv[i]);
            continue; // Skip this argument if invalid.
        }
        // Calculate the length of the name (part before `=`).
        name_len = last - argv[i];

        // Ensure that the name is not empty.
        if (name_len == 0) {
            printf("Invalid format: '%s'. Name cannot be empty.\n", argv[i]);
            continue; // Skip this argument if the name is empty.
        }
        // Set the starting index of the value (part after `=`).
        value_start = (last + 1) - argv[i];
        // Copy the name into the `name` buffer.
        strncpy(name, argv[i], name_len);
        name[name_len] = '\0'; // Null-terminate the name string.
        // Expand the environmental variables inside the value part of the argument.
        __expand_env(&argv[i][value_start], value, BUFSIZ);
        // Try to set the environmental variable if both name and value are valid.
        if ((strlen(name) > 0) && (strlen(value) > 0)) {
            if (setenv(name, value, 1) == -1) {
                printf("Failed to set environmental variable: %s\n", name);
                return 1; // Return 1 on failure to set the environment variable.
            }
        } else {
            printf("Invalid variable assignment: '%s'. Name and value must be non-empty.\n", argv[i]);
        }
    }
    return 0; // Return 0 on success.
}

/// @brief Changes the current working directory.
/// @param argc The number of arguments passed.
/// @param argv The array of arguments, where argv[0] is the command and argv[1] is the target directory.
/// @return Returns 0 on success, 1 on failure.
static int __cd(int argc, char *argv[])
{
    // Check if too many arguments are provided.
    if (argc > 2) {
        puts("cd: too many arguments\n");
        return 1;
    }
    // Determine the path to change to.
    const char *path = NULL;
    if (argc == 2) {
        path = argv[1];
    } else {
        // If no argument is provided, use the HOME directory.
        path = getenv("HOME");
        if (path == NULL) {
            puts("cd: There is no home directory set.\n");
            return 1;
        }
    }
    // Get the real path of the target directory.
    char real_path[PATH_MAX];
    if (realpath(path, real_path, PATH_MAX) != real_path) {
        printf("cd: Failed to resolve directory '%s': %s\n", path, strerror(errno));
        return 1;
    }
    // Stat the directory to ensure it exists and get information about it.
    stat_t dstat;
    if (stat(real_path, &dstat) == -1) {
        printf("cd: cannot stat '%s': %s\n", real_path, strerror(errno));
        return 1;
    }
    // Check if the path is a symbolic link.
    if (S_ISLNK(dstat.st_mode)) {
        char link_buffer[PATH_MAX];
        ssize_t len;
        // Read the symbolic link.
        if ((len = readlink(real_path, link_buffer, sizeof(link_buffer) - 1)) < 0) {
            printf("cd: Failed to read symlink '%s': %s\n", real_path, strerror(errno));
            return 1;
        }
        // Null-terminate the link buffer.
        link_buffer[len] = '\0';
        // Resolve the symlink to an absolute path.
        if (realpath(link_buffer, real_path, PATH_MAX) != real_path) {
            printf("cd: Failed to resolve symlink '%s': %s\n", link_buffer, strerror(errno));
            return 1;
        }
    }
    // Open the directory to ensure it's accessible.
    int fd = open(real_path, O_RDONLY | O_DIRECTORY, S_IXUSR | S_IXOTH);
    if (fd == -1) {
        printf("cd: %s: %s\n", real_path, strerror(errno));
        return 1;
    }
    // Change the current working directory.
    if (chdir(real_path) == -1) {
        printf("cd: Failed to change directory to '%s': %s\n", real_path, strerror(errno));
        close(fd);
        return 1;
    }
    // Close the directory file descriptor.
    close(fd);
    // Get the updated current working directory.
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        printf("cd: Failed to get current working directory: %s\n", strerror(errno));
        return 1;
    }
    // Update the PWD environment variable to the new directory.
    if (setenv("PWD", cwd, 1) == -1) {
        printf("cd: Failed to set current working directory in environment: %s\n", strerror(errno));
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
/// @return Returns 1 if the entry was successfully added, 0 if it was a duplicate.
static inline int __history_push(rb_history_entry_t *entry)
{
    rb_history_entry_t previous_entry;
    // Validate input parameter.
    if (entry == NULL) {
        pr_crit("__history_push: Invalid entry.\n");
        return 0;
    }
    // Check if there's an existing entry at the back of the history.
    if (!rb_history_peek_back(&history, &previous_entry)) {
        // Compare the new entry with the last entry to avoid duplicates.
        if (strcmp(entry->buffer, previous_entry.buffer) == 0) {
            // Return 0 if the new entry is the same as the previous one (duplicate).
            return 0;
        }
    }
    // Push the new entry to the back of the history ring buffer.
    rb_history_push_back(&history, entry);
    // Set the history index to the current count, pointing to the end.
    history_index = history.count;
    // Return 1 to indicate the new entry was successfully added.
    return 1;
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

/// @brief Clears the current command from both the display and the buffer.
/// @param entry The history entry containing the command buffer.
/// @param index The current index in the command buffer.
/// @param length The total length of the command buffer.
static inline void __command_clear(rb_history_entry_t *entry, int *index, int *length)
{
    // Validate the input parameters to avoid null pointer dereference.
    if ((entry == NULL) || (index == NULL) || (length == NULL)) {
        pr_crit("Invalid parameters passed to __command_clear.\n");
        return;
    }
    // Ensure index and length are greater than zero and index does not exceed
    // length.
    if ((*index < 0) || (*length < 0) || (*index > *length)) {
        pr_crit("Invalid index or length values: index=%d, length=%d.\n", *index, *length);
        return;
    }
    // Move the cursor to the end of the current command.
    printf("\033[%dC", (*length) - (*index));
    // Clear the current command from the display by moving backwards.
    while ((*length)--) { putchar('\b'); }
    // Clear the current command from the buffer by setting it to zero.
    memset(entry->buffer, 0, entry->size);
    // Reset both index and length to zero, as the command is now cleared.
    (*index) = (*length) = 0;
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
            pr_debug("[%2d] '%c'\n", i, filename[i], entry->buffer);
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
    pr_debug("__command_complete(%s, %2d, %2d)\n", entry->buffer, *index, *length);

    char cwd[PATH_MAX]; // Buffer to store the current working directory.
    int words;          // Variable to store the word count.
    dirent_t dent;      // Prepare the dirent variable.

    // Count the number of words in the command buffer.
    words = __count_words(entry->buffer);

    // If there are no words in the command buffer, log it and return.
    if (words == 0) {
        pr_debug("__command_complete(%s, %2d, %2d) : No words.\n", entry->buffer, *index, *length);
        return 0;
    }

    // Determines if we are at the beginning of a new argument, last character is space.
    if (__is_separator(entry->buffer[(*index) - 1])) {
        pr_debug("__command_complete(%s, %2d, %2d) : Separator.\n", entry->buffer, *index, *length);
        return 0;
    }

    // If the last two characters are two dots `..` append a slash `/`, and
    // continue.
    if (((*index) >= 2) && ((entry->buffer[(*index) - 1] == '.') && (entry->buffer[(*index) - 2] == '.'))) {
        pr_debug("__command_complete(%s, %2d, %2d) : Append '/'.\n", entry->buffer, *index, *length);
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
            pr_debug("__command_complete(%s, %2d, %2d) : Suggest run '%s' -> '%s'.\n", entry->buffer, *index, *length,
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
            pr_debug("__command_complete(%s, %2d, %2d) : Suggest abs '%s' -> '%s'.\n", entry->buffer, *index, *length,
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
            pr_debug("__command_complete(%s, %2d, %2d) : Suggest in path '%s' -> '%s'.\n", entry->buffer, *index, *length,
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
                pr_debug("__command_complete(%s, %2d, %2d) : Suggest 1 '%s' -> '%s'.\n", entry->buffer, *index, *length,
                         last_argument, dent.d_name);
                __command_suggest(
                    dent.d_name,
                    dent.d_type,
                    strlen(_basename),
                    entry,
                    index,
                    length);
            }
        } else if (*_basename != 0) {
            if (__folder_contains(cwd, _basename, 0, &dent)) {
                pr_debug("__command_complete(%s, %2d, %2d) : Suggest 2 '%s' -> '%s'.\n", entry->buffer, *index, *length,
                         last_argument, dent.d_name);
                __command_suggest(
                    dent.d_name,
                    dent.d_type,
                    strlen(_basename),
                    entry,
                    index,
                    length);
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
                    __command_clear(entry, &index, &length);
                    // Fetch the history element.
                    rb_history_entry_t *history_entry = __history_fetch(c);
                    if (history_entry) {
                        // Sets the command.
                        rb_history_entry_copy(entry->buffer, history_entry->buffer, entry->size);
                        // Print the old command.
                        printf(entry->buffer);
                        // Set index to the end.
                        index = length = strnlen(entry->buffer, entry->size);
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
                    __command_clear(entry, &index, &length);
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

/// @brief Allocates and parses the arguments (argv) from the provided command string.
/// @param command The executed command.
/// @param argc Pointer to the argument count (to be set by this function).
/// @param argv Pointer to the argument list (to be set by this function).
static void __alloc_argv(char *command, int *argc, char ***argv)
{
    // Input validation: Check if command, argc, or argv are NULL.
    if (command == NULL || argc == NULL || argv == NULL) {
        printf("Error: Invalid input. 'command', 'argc', or 'argv' is NULL.\n");
        return;
    }
    // Count the number of words (arguments) in the command.
    (*argc) = __count_words(command);
    // If there are no arguments, return early.
    if ((*argc) == 0) {
        *argv = NULL;
        return;
    }
    // Allocate memory for the arguments array (argc + 1 for the NULL terminator).
    (*argv) = (char **)malloc(sizeof(char *) * ((*argc) + 1));
    if (*argv == NULL) {
        printf("Error: Failed to allocate memory for arguments.\n");
        return;
    }
    bool_t inword  = false;
    char *cit      = command; // Command iterator.
    char *argStart = command; // Start of the current argument.
    size_t argcIt  = 0;       // Iterator for arguments.
    // Iterate through the command string.
    do {
        // Check if the current character is not a separator (indicating part of a word).
        if (!__is_separator(*cit)) {
            if (!inword) {
                // If we are entering a new word, mark the start of the argument.
                argStart = cit;
                inword   = true;
            }
            continue;
        }
        // If we were inside a word and encountered a separator, process the word.
        if (inword) {
            inword = false;
            // Expand possible environment variables in the current argument.
            char expand_env_buf[BUFSIZ];
            ___expand_env(argStart, expand_env_buf, BUFSIZ, cit - argStart, true);
            // Allocate memory for the expanded argument.
            (*argv)[argcIt] = (char *)malloc(strlen(expand_env_buf) + 1);
            if ((*argv)[argcIt] == NULL) {
                printf("Error: Failed to allocate memory for argument %zu.\n", argcIt);
                // Free previously allocated arguments to prevent memory leaks.
                for (size_t j = 0; j < argcIt; ++j) {
                    free((*argv)[j]);
                }
                free(*argv);
                *argv = NULL;
                return;
            }
            // Copy the expanded argument to the argv array.
            strcpy((*argv)[argcIt++], expand_env_buf);
        }
    } while (*cit++);
    // Null-terminate the argv array.
    (*argv)[argcIt] = NULL;
}

/// @brief Frees the memory allocated for argv and its arguments.
/// @param argc The number of arguments.
/// @param argv The array of argument strings to be freed.
static inline void __free_argv(int argc, char **argv)
{
    // Input validation: Check if argv is NULL before proceeding.
    if (argv == NULL) {
        return;
    }
    // Free each argument in the argv array.
    for (int i = 0; i < argc; ++i) {
        if (argv[i] != NULL) {
            free(argv[i]);
        }
    }
    // Free the argv array itself.
    free(argv);
}

/// @brief Sets up file redirections based on arguments.
/// @param argcp Pointer to the argument count (to be updated if redirects are removed).
/// @param argvp Pointer to the argument list (to be updated if redirects are removed).
static void __setup_redirects(int *argcp, char ***argvp)
{
    char **argv = *argvp;
    int argc    = *argcp;

    char *path;
    int flags   = O_CREAT | O_WRONLY;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

    bool_t rd_stdout = false;
    bool_t rd_stderr = false;

    // Iterate through arguments to find redirects.
    for (int i = 1; i < argc - 1; ++i) {
        // Skip if the argument doesn't contain '>'.
        if (!strstr(argv[i], ">")) {
            continue;
        }
        // Check if the next argument (i + 1) is within bounds.
        if (i + 1 >= argc || argv[i + 1] == NULL) {
            printf("Error: Missing path for redirection after '%s'.\n", argv[i]);
            exit(1); // Exit if no path is provided for redirection.
        }
        path = argv[i + 1]; // Set the path for redirection.
        // Determine the stream to redirect based on the first character of the argument.
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
            continue; // If no valid redirection is found, continue.
        }
        // Determine the open flags for append or truncate.
        if (strstr(argv[i], ">>")) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        // Remove redirection arguments from argv.
        *argcp -= 2;
        free(argv[i]);
        (*argvp)[i] = NULL;
        free(argv[i + 1]);
        (*argvp)[i + 1] = NULL;
        // Open the file for redirection.
        int fd = open(path, flags, mode);
        if (fd < 0) {
            printf("Error: Failed to open file '%s' for redirection.\n", path);
            exit(1);
        }
        // Redirect stdout if necessary.
        if (rd_stdout) {
            close(STDOUT_FILENO);
            if (dup(fd) < 0) {
                printf("Error: Failed to redirect stdout to file '%s'.\n", path);
                close(fd);
                exit(1);
            }
        }
        if (rd_stderr) {
            close(STDERR_FILENO);
            if (dup(fd) < 0) {
                printf("Error: Failed to redirect stderr to file '%s'.\n", path);
                close(fd);
                exit(1);
            }
        }
        close(fd); // Close the file descriptor after redirection.
        break;     // Stop after handling one redirection.
    }
} /// @brief Executes the command stored in the history entry.
/// @param entry The history entry containing the command.
/// @return Returns the exit status of the command.
static int __execute_command(rb_history_entry_t *entry)
{
    int _status = 0;

    // Retrieve the arguments from the command buffer.
    int _argc = 1; // Initialize the argument count.
    char **_argv;  // Argument vector.

    __alloc_argv(entry->buffer, &_argc, &_argv);

    // Check if the command is empty (no arguments parsed).
    if (_argc == 0) {
        return 0;
    }

    // Handle built-in commands.
    if (!strcmp(_argv[0], "init")) {
        // Placeholder for the 'init' command.
    } else if (!strcmp(_argv[0], "cd")) {
        // Execute the 'cd' command.
        __cd(_argc, _argv);
    } else if (!strcmp(_argv[0], "..")) {
        // Shortcut for 'cd ..'.
        const char *__argv[] = { "cd", "..", NULL };
        __cd(2, (char **)__argv);
    } else if (!strcmp(_argv[0], "export")) {
        // Execute the 'export' command.
        __export(_argc, _argv);
    } else {
        // Handle external commands (executed as child processes).
        bool_t blocking = true;
        // Check if the command should be run in the background (indicated by '&').
        if (strcmp(_argv[_argc - 1], "&") == 0) {
            blocking = false;   // Non-blocking execution (background process).
            _argc--;            // Remove the '&' from the argument list.
            free(_argv[_argc]); // Free the memory for the '&'.
            _argv[_argc] = NULL;
        }
        // Block SIGCHLD signal to prevent interference with child processes.
        __block_sigchld();
        // Fork the current process to create a child process.
        pid_t cpid = fork();
        if (cpid == 0) {
            // Child process: Execute the command.
            pid_t pid = getpid();
            setpgid(cpid, pid);  // Make the new process a group leader.
            __unblock_sigchld(); // Unblock SIGCHLD signals in the child process.
            // Handle redirections (e.g., stdout, stderr).
            __setup_redirects(&_argc, &_argv);
            // Attempt to execute the command using execvp.
            if (execvp(_argv[0], _argv) == -1) {
                printf("\nUnknown command: %s\n", _argv[0]);
                exit(127); // Exit with status 127 if the command is not found.
            }
        }
        if (blocking) {
            // Parent process: Wait for the child process to finish.
            waitpid(cpid, &_status, 0);
            // Handle different exit statuses of the child process.
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
        __unblock_sigchld(); // Unblock SIGCHLD signals after command execution.
    }
    // Free the memory allocated for the argument list.
    __free_argv(_argc, _argv);
    // Update the global status variable with the exit status of the command.
    status = WEXITSTATUS(_status);
    return status; // Return the exit status of the command.
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
