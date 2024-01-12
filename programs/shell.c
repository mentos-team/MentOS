/// @file shell.c
/// @brief Implement shell functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

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

/// Maximum length of commands.
#define CMD_LEN 64
/// Maximum lenght of the history.
#define HISTORY_MAX 10

// Required by `export`
#define ENV_NORM 1
#define ENV_BRAK 2
#define ENV_PROT 3

// The input command.
static char cmd[CMD_LEN] = { 0 };
// The index of the cursor.
static size_t cmd_cursor_index = 0;
// History of commands.
static char history[HISTORY_MAX][CMD_LEN] = { 0 };
// The current write index inside the history.
static int history_write_index = 0;
// The current read index inside the history.
static int history_read_index = 0;
// Boolean used to check if the history is full.
static bool_t history_full = false;

static inline int __is_separator(char c)
{
    return ((c == '\0') || (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r'));
}

static inline int __count_words(const char *sentence)
{
    int result     = 0;
    bool_t inword  = false;
    const char *it = sentence;
    do {
        if (__is_separator(*it)) {
            if (inword) {
                inword = false;
                result++;
            }
        } else {
            inword = true;
        }
    } while (*it++);
    return result;
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
    size_t entry_len = strlen(entry);
    int found        = 0;
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
        if (__folder_contains(token, entry, DT_REG, result))
            return 1;
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
    printf(FG_GREEN "%s" FG_WHITE "@" FG_CYAN "%s " FG_BLUE_BRIGHT "[%02d:%02d:%02d]" FG_WHITE " [%s] " FG_WHITE_BRIGHT "\n-> %% ",
           USER, HOSTNAME, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, CWD);
}

static void __expand_env(char *str, char *buf, size_t buf_len)
{
    // Buffer where we store the name of the variable.
    char buffer[BUFSIZ] = { 0 };
    // Flags used to keep track of the special characters.
    unsigned flags = 0;
    // We keep track of where teh
    char *env_start = NULL;
    // Where we store the retrieved environmental variable value.
    char *ENV = NULL;
    // Get the length of the string.
    size_t str_len = strlen(str);
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
                if ((ENV = getenv(buffer)))
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
                if ((ENV = getenv(buffer)))
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
        strcpy(buffer, env_start);
        // Search for the environmental variable, and print it.
        if ((ENV = getenv(buffer)))
            for (int k = 0; k < strlen(ENV); ++k)
                buf[b_pos++] = ENV[k];
        // Remove the flag.
        bit_clear_assign(flags, ENV_NORM);
    }
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
        printf("%s: too many arguments\n\n", argv[0]);
        return 1;
    }
    const char *path = NULL;
    if (argc == 2) {
        path = argv[1];
    } else {
        path = getenv("HOME");
        if (path == NULL) {
            printf("cd: There is no home directory set.\n\n");
            return 1;
        }
    }
    int fd = open(path, O_RDONLY | O_DIRECTORY, S_IXUSR);
    if (fd == -1) {
        printf("cd: %s\n\n", strerror(errno), path);
        return 1;
    }
    // Set current working directory.
    chdir(path);
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

/// @brief Push the command inside the history.
static inline void __hst_push(char *_cmd)
{
    // Reset the read index.
    history_read_index = history_write_index;
    // Check if it is a duplicated entry.
    if (history_write_index > 0) {
        if (strcmp(history[history_write_index - 1], _cmd) == 0) {
            return;
        }
    }
    // Insert the node.
    strcpy(history[history_write_index], _cmd);
    if (++history_write_index >= HISTORY_MAX) {
        history_write_index = 0;
        history_full        = true;
    }
    // Reset the read index.
    history_read_index = history_write_index;
}

/// @brief Give the key allows to navigate through the history.
static char *__hst_fetch(bool_t up)
{
    if ((history_write_index == 0) && (history_full == false)) {
        return NULL;
    }
    // If the history is empty do nothing.
    char *_cmd = NULL;
    // Update the position inside the history.
    int next_index = history_read_index + (up ? -1 : +1);
    // Check the next index.
    if (history_full) {
        if (next_index < 0) {
            next_index = HISTORY_MAX - 1;
        } else if (next_index >= HISTORY_MAX) {
            next_index = 0;
        }
        // Do no read where ne will have to write next.
        if (next_index == history_write_index) {
            next_index = history_read_index;
            return NULL;
        }
    } else {
        if (next_index < 0) {
            next_index = 0;
        } else if (next_index >= history_write_index) {
            next_index = history_read_index;
            return NULL;
        }
    }
    history_read_index = next_index;
    _cmd               = history[history_read_index];
    // Return the command.
    return _cmd;
}

/// @brief Completely delete the current command.
static inline void __cmd_clr(void)
{
    // First we need to get back to the end of the line.
    while (cmd[cmd_cursor_index] != 0) {
        ++cmd_cursor_index;
        puts("\033[1C");
    }
    // Then we delete all the character.
    for (size_t it = 0; it < cmd_cursor_index; ++it) {
        putchar('\b');
    }
    // Reset the index.
    cmd_cursor_index = 0;
}

/// @brief Sets the new command.
static inline void __cmd_set(char *_cmd)
{
    // Outputs the command.
    printf(_cmd);
    // Moves the cursore.
    cmd_cursor_index += strlen(_cmd);
    // Copies the command.
    strcpy(cmd, _cmd);
}

/// @brief Erases one character from the console.
static inline void __cmd_ers(char c)
{
    if ((c == '\b') && (cmd_cursor_index > 0)) {
        strcpy(cmd + cmd_cursor_index - 1, cmd + cmd_cursor_index);
        putchar('\b');
        --cmd_cursor_index;
    } else if ((c == 0x7F) && (cmd[0] != 0) && ((cmd_cursor_index + 1) < CMD_LEN)) {
        strcpy(cmd + cmd_cursor_index, cmd + cmd_cursor_index + 1);
        putchar(0x7F);
    }
}

/// @brief Appends the character `c` on the command.
static inline int __cmd_app(char c)
{
    if ((cmd_cursor_index + 1) < CMD_LEN) {
        // If at the current index there is a character, shift the entire
        // command ahead.
        if (cmd[cmd_cursor_index] != 0) {
            // Move forward the entire string.
            for (unsigned long i = strlen(cmd); i > cmd_cursor_index; --i) {
                cmd[i] = cmd[i - 1];
            }
        }
        // Place the new character.
        cmd[cmd_cursor_index++] = c;
        return 1;
    }
    return 0;
}

static inline void __cmd_sug(dirent_t *suggestion, size_t starting_position)
{
    if (suggestion) {
        for (size_t i = starting_position; i < strlen(suggestion->d_name); ++i) {
            if (__cmd_app(suggestion->d_name[i])) {
                putchar(suggestion->d_name[i]);
            }
        }
        // If we suggested a directory, append a slash.
        if (suggestion->d_type == DT_DIR) {
            if (__cmd_app('/')) {
                putchar('/');
            }
        }
    }
}

/// @brief Gets the inserted command.
static void __cmd_get(void)
{
    // Re-Initialize the cursor index.
    cmd_cursor_index = 0;
    // Initializing the current command line buffer
    memset(cmd, '\0', CMD_LEN);
    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    __set_echo(false);
    do {
        int c = getchar();
        // Return Key
        if (c == '\n') {
            putchar('\n');
            // Break the while loop.
            break;
        }
        // It is a special character.
        if (c == '\033') {
            c = getchar();
            if (c == '[') {
                c = getchar(); // Get the char.
                if ((c == 'A') || (c == 'B')) {
                    char *old_cmd = __hst_fetch(c == 'A');
                    if (old_cmd != NULL) {
                        // Clear the current command.
                        __cmd_clr();
                        // Sets the command.
                        __cmd_set(old_cmd);
                    }
                } else if (c == 'D') {
                    if (cmd_cursor_index > 0) {
                        --cmd_cursor_index;
                        puts("\033[1D");
                    }
                } else if (c == 'C') {
                    if ((cmd_cursor_index + 1) < CMD_LEN && (cmd_cursor_index + 1) <= strlen(cmd)) {
                        ++cmd_cursor_index;
                        puts("\033[1C");
                    }
                } else if (c == 'H') {
                    // Move the cursor back to the beginning.
                    printf("\033[%dD", cmd_cursor_index);
                    // Reset the cursor position.
                    cmd_cursor_index = 0;
                } else if (c == 'F') {
                    // Compute the offest to the end of the line, and move only if necessary.
                    size_t offset = strlen(cmd) - cmd_cursor_index;
                    if (offset > 0) {
                        printf("\033[%dC", offset);
                        // Reset the cursor position.
                        cmd_cursor_index += offset;
                    }
                } else if (c == '3') {
                    c = getchar(); // Get the char.
                    if (c == '~') {
                        __cmd_ers(0x7F);
                    }
                }
            }
        } else if (c == '\b') {
            __cmd_ers('\b');
        } else if (c == '\t') {
            // Get the lenght of the command.
            size_t cmd_len = strlen(cmd);
            // Count the number of words.
            int words = __count_words(cmd);
            // If there are no words, skip.
            if (words == 0) {
                continue;
            }
            // Determines if we are at the beginning of a new argument, last character is space.
            if (__is_separator(cmd[cmd_len - 1])) {
                continue;
            }
            // If the last two characters are two dots `..` append a slash `/`,
            // and continue.
            if ((cmd_len >= 2) && ((cmd[cmd_len - 2] == '.') && (cmd[cmd_len - 1] == '.'))) {
                if (__cmd_app('/')) {
                    putchar('/');
                    continue;
                }
            }
            // Determines if we are executing a command from current directory.
            int is_run_cmd = (words == 1) && (cmd[0] == '.') && (cmd_len > 3) && (cmd[1] == '/');
            // Determines if we are entering an absolute path.
            int is_abs_path = (words == 1) && (cmd[0] == '/');
            // Prepare the dirent variable.
            dirent_t dent;
            // If there is only one word, we are searching for a command.
            if (is_run_cmd) {
                if (__folder_contains(cwd, cmd + 2, 0, &dent)) {
                    __cmd_sug(&dent, cmd_len - 2);
                }
            } else if (is_abs_path) {
                char _dirname[PATH_MAX];
                if (!dirname(cmd, _dirname, sizeof(_dirname))) {
                    continue;
                }
                const char *_basename = basename(cmd);
                if (!_basename) {
                    continue;
                }
                if ((*_dirname == 0) || (*_basename == 0)) {
                    continue;
                }
                if (__folder_contains(_dirname, _basename, 0, &dent)) {
                    __cmd_sug(&dent, strlen(_basename));
                }
            } else if (words == 1) {
                if (__search_in_path(cmd, &dent)) {
                    __cmd_sug(&dent, cmd_len);
                }
            } else {
                // Search the last occurrence of a space, from there on
                // we will have the last argument.
                char *last_argument = strrchr(cmd, ' ');
                // We need to move ahead of one character if we found the space.
                last_argument = last_argument ? last_argument + 1 : NULL;
                // If there is no last argument.
                if (last_argument == NULL) {
                    continue;
                }
                char _dirname[PATH_MAX];
                if (!dirname(last_argument, _dirname, sizeof(_dirname))) {
                    continue;
                }
                const char *_basename = basename(last_argument);
                if (!_basename) {
                    continue;
                }
                if ((*_dirname != 0) && (*_basename != 0)) {
                    if (__folder_contains(_dirname, _basename, 0, &dent)) {
                        __cmd_sug(&dent, strlen(_basename));
                    }
                } else if (*_basename != 0) {
                    if (__folder_contains(cwd, _basename, 0, &dent)) {
                        __cmd_sug(&dent, strlen(_basename));
                    }
                }
            }
        } else if (c == 127) {
            if ((cmd_cursor_index + 1) <= strlen(cmd)) {
                strcpy(cmd + cmd_cursor_index, cmd + cmd_cursor_index + 1);
                putchar(127);
            }
        } else if (iscntrl(c)) {
            if (c == CTRL('C')) {
                // Re-set the index to the beginning.
                cmd_cursor_index = 0;
                // Go to the new line.
                printf("\n\n");
                // Sets the command.
                __cmd_set("\0");
                // Break the while loop.
                break;
            } else if (c == CTRL('U')) {
                // Clear the current command.
                __cmd_clr();
                // Re-set the index to the beginning.
                cmd_cursor_index = 0;
                // Sets the command.
                __cmd_set("\0");
            } else if (c == CTRL('D')) {
                // Go to the new line.
                printf("\n");
                exit(0);
            }
        } else if ((c > 0) && (c != '\n')) {
            if (__cmd_app(c)) {
                putchar(c);
            }
        } else {
            pr_debug("Unrecognized character %02x (%c)\n", c, c);
        }
    } while (cmd_cursor_index < CMD_LEN);

    // Cleans all blanks at the beginning of the command.
    trim(cmd);
    __set_echo(true);
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
    (*argv)         = (char **)malloc(sizeof(char *) * ((*argc) + 1));
    bool_t inword   = false;
    const char *cit = command;
    size_t argcIt = 0, argIt = 0;
    do {
        if (__is_separator(*cit)) {
            if (inword) {
                inword                   = false;
                (*argv)[argcIt++][argIt] = '\0';
                argIt                    = 0;
            }
        } else {
            // Allocate string for argument.
            if (!inword) {
                (*argv)[argcIt] = (char *)malloc(sizeof(char) * CMD_LEN);
            }
            inword                   = true;
            (*argv)[argcIt][argIt++] = (*cit);
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

static void __setup_redirects(int *argcp, char ***argvp) {
    char **argv = *argvp;
    int argc = *argcp;

    char* path;
    int flags = O_CREAT | O_WRONLY;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

    bool_t rd_stdout, rd_stderr;
    rd_stdout = rd_stderr = 0;

    for (int i = 1; i < argc - 1; ++i) {
        if (!strstr(argv[i], ">")) {
            continue;
        }

        path = argv[i + 1];

        // Determine stream to redirect
        switch(*argv[i]) {
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
        free(argv[i+1]);
        (*argvp)[i+1] = 0;

        int fd = open(path, flags, mode);
        if (fd < 0) {
            printf("\n%s: Failed to open file\n", path);
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

static int __execute_cmd(char* command, bool_t add_to_history)
{
    int status = 0;
    // Retrieve the options from the command.
    // The current number of arguments.
    int _argc = 1;
    // The vector of arguments.
    char **_argv;
    __alloc_argv(command, &_argc, &_argv);
    // Check if the command is empty.
    if (_argc == 0) {
        return 0;
    }

    // Add the command to the history.
    if (add_to_history) {
        __hst_push(cmd);
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
        // Is a shell path, execute it!
        pid_t cpid = fork();
        if (cpid == 0) {
            // Makes the new process a group leader
            pid_t pid = getpid();
            setpgid(cpid, pid);

            __setup_redirects(&_argc, &_argv);

            if (execvp(_argv[0], _argv) == -1) {
                printf("\nUnknown command: %s\n", _argv[0]);
                exit(1);
            }
        }
        if (blocking) {
            waitpid(cpid, &status, 0);
            if (WIFSIGNALED(status)) {
                printf(FG_RED "\nExit status %d, killed by signal %d\n" FG_RESET, WEXITSTATUS(status), WTERMSIG(status));
            } else if (WIFSTOPPED(status)) {
                printf(FG_YELLOW "\nExit status %d, stopped by signal %d\n" FG_RESET, WEXITSTATUS(status), WSTOPSIG(status));
            }
        }
    }
    // Free up the memory reserved for the arguments.
    __free_argv(_argc, _argv);
    return status;
}

static void __interactive_mode(void)
{
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while (true) {
        // First print the prompt.
        __prompt_print();
        // Get the input command.
        __cmd_get();
        __execute_cmd(cmd, true);
    }
#pragma clang diagnostic pop
}

static int __execute_file(char *path)
{
    int status = 0;
    int fd;
    if ((fd = open(path, O_RDONLY, 0)) == -1) {
        printf("\n%s: Failed to open file\n", path);
        exit(1);
    }
    while (fgets(cmd, sizeof(cmd), fd)) {
        if (cmd[0] == '#') {
            continue;
        }

        if ((status = __execute_cmd(cmd, false)) != 0) {
            printf("\n%s: exited with %d\n", cmd, status);
        }
    }

    return status;
}

void wait_for_child(int signum)
{
    wait(NULL);
}

int main(int argc, char *argv[])
{
    setsid();

    struct termios _termios;
    tcgetattr(STDIN_FILENO, &_termios);
    _termios.c_lflag &= ~ISIG;
    tcsetattr(STDIN_FILENO, 0, &_termios);

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

    // Interactive
    if (argc < 2) {
        // Move inside the home directory.
        __cd(0, NULL);
        __interactive_mode();
    } else {
        // check file arguments
        for (int i = 1; i < argc; ++i) {
            stat_t buf;
            if (stat(argv[i], &buf) < 0) {
                printf("\n%s: No such file\n", argv[i]);
                exit(1);
            }
        }

        for (int i = 1; i < argc; ++i) {
            int status;
            if (!(status = __execute_file(argv[i]))) {
                return status;
            }
        }
    }

    return 0;
}
