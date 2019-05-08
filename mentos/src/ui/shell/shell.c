///                MentOS, The Mentoring Operating system project
/// @file shell.c
/// @brief Implement shell functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "wait.h"
#include "video.h"
#include "types.h"
#include "stdio.h"
#include "debug.h"
#include "panic.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "version.h"
#include "process.h"
#include "keyboard.h"
#include "commands.h"
#include "shell_login.h"

#define HISTORY_MAX 10

/// The current user.
userenv_t current_user;

/// The input command.
static char cmd[CMD_LEN];

/// The index of the cursor.
static uint32_t cmd_cursor_index;

/// History of commands.
char history[HISTORY_MAX][CMD_LEN];

///
static int history_write_index = 0;

///
static int history_read_index = 0;

static bool_t history_full = false;

#define MAX_NUM_COM 50 ///< Maximum number of saved commands.
struct {
	/// The name of the command.
	char cmdname[CMD_LEN];
	/// The function pointer to the command.
	CommandFunction function;
	/// The description of the command.
	char cmddesc[DESC_LEN];
} _shell_commands[] = {
	{ "logo", cmd_logo, "Show an ascii art logo" },
	{ "clear", cmd_clear, "Clear the screen" },
	{ "echo", cmd_echo, "Print some lines of text" },
	{ "poweroff", cmd_poweroff, "Turn off the machine" },
	{ "uname", cmd_uname,
	  "Print kernel version, try uname --help for more info" },
	{ "credits", cmd_credits, "Show " OS_NAME " credits" },
	{ "sleep", cmd_sleep, "Pause the OS for a particular number of seconds" },
	{ "cpuid", cmd_cpuid, "Show cpu identification informations" },
	{ "help", cmd_help, "See the 'help' list to learn commands now available" },
	{ "ls", cmd_ls, "Tool for listing dir - not complete-" },
	{ "cd", cmd_cd, "Change dir - not complete-" },
	{ "mkdir", cmd_mkdir, "Creates a new directory." },
	{ "rm", cmd_rm, "Removes a file." },
	{ "rmdir", cmd_rmdir, "Removes a directory." },
	{ "whoami", cmd_whoami, "Show the current user name" },
	{ "pwd", cmd_pwd, "Print current working directory" },
	{ "more", cmd_more, "Read content of a file" },
	{ "touch", cmd_touch, "Create a new file" },
	{ "newfile", cmd_newfile, "Create a new file" },
	{ "ps", cmd_ps, "Show task list" },
	{ "date", cmd_date, "Show date and time" },
	{ "clear", cmd_clear, "Clears the screen" },
	{ "showpid", cmd_showpid, "Shows the PID of the shell" },
	{ "history", cmd_show_history, "Shows the shell history" },
	{ "nice", cmd_nice, "Change the nice value of the process" }
};

/// @brief Completely delete the current command.
static void shell_command_clear()
{
	for (size_t it = 0; it < cmd_cursor_index; ++it) {
		putchar('\b');
	}
	cmd_cursor_index = 0;
}

///
/// @brief
/// @param _cmd
static void shell_command_set(char *_cmd)
{
	// Outputs the command.
	printf(_cmd);
	// Moves the cursore.
	cmd_cursor_index += strlen(_cmd);
	// Copies the command.
	strcpy(cmd, _cmd);
}

static void shell_command_erase_char()
{
	if (cmd_cursor_index > 0) {
		cmd[--cmd_cursor_index] = '\0';
	}
}

static bool_t shell_command_append_char(char c)
{
	if ((cmd_cursor_index + 1) < CMD_LEN) {
		cmd[cmd_cursor_index++] = c;
		cmd[cmd_cursor_index] = '\0';

		return true;
	}

	return false;
}

static inline void history_debug_print()
{
#if 1
	// Prints the history stack with current indexes values.
	dbg_print("------------------------------\n");
	for (size_t index = 0; index < HISTORY_MAX; ++index) {
		dbg_print("[%d]%c%c: %s\n", index,
				  (index == history_write_index) ? 'w' : ' ',
				  (index == history_read_index) ? 'r' : ' ', history[index]);
	}
#endif
}

/// @brief Push the command inside the history.
static void history_push(char *_cmd)
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
		history_full = true;
	}
	// Reset the read index.
	history_read_index = history_write_index;
	history_debug_print();
}

/// @brief Give the key allows to navigate through the history.
static char *history_fetch(const int key)
{
	if ((history_write_index == 0) && (history_full == false)) {
		return NULL;
	}
	// If the history is empty do nothing.
	char *_cmd = NULL;
	int next_index = history_read_index;
	// Update the position inside the history.
	if (key == KEY_DOWN) {
		++next_index;
	} else if (key == KEY_UP) {
		--next_index;
	}
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
		}
	} else {
		if (next_index < 0) {
			next_index = 0;
		} else if (next_index >= history_write_index) {
			next_index = history_read_index;
		}
	}
	history_read_index = next_index;
	_cmd = history[history_read_index];
	history_debug_print();
	// Return the command.
	return _cmd;
}

void cmd_show_history(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	// Prints the history stack with current indexes values
	printf("------------------------------\n");
	printf("        Debug history         \n");
	printf("------------------------------\n");
	for (size_t index = 0; index < HISTORY_MAX; ++index) {
		printf("[%d]%c%c: %s\n", index,
			   (index == history_write_index) ? 'w' : ' ',
			   (index == history_read_index) ? 'r' : ' ', history[index]);
	}
}

/// @brief Prints the prompt.
static void shell_print_prompt()
{
	video_set_color(BRIGHT_BLUE);
	printf(current_user.username);
	video_set_color(WHITE);
	char cwd[MAX_PATH_LENGTH];
	getcwd(cwd, MAX_PATH_LENGTH);
	printf("~:%s# ", cwd);
	// Update the lower-bounds for the video.
	lower_bound_x = video_get_column();
	lower_bound_y = video_get_line();
}

/// @brief Gets the inserted command.
static void shell_get_command()
{
	// Re-Initialize the cursor index.
	cmd_cursor_index = 0;
	//Initializing the current command line buffer
	memset(cmd, '\0', CMD_LEN);
	do {
		int c = getchar();
		// Return Key
		if (c == '\n') {
			if (strlen(cmd) == 0) {
				printf("\n");
			}
			// Break the while loop.
			break;
		} else if (c == '\033') {
			getchar(); // Skip '['
			c = getchar(); // Get the char.
			char *_cmd = NULL;
			if ((c == KEY_UP) || (c == KEY_DOWN)) {
				_cmd = history_fetch(c);
			}
			if (_cmd != NULL) {
				// Clear the current command.
				shell_command_clear();
				// Sets the command.
				shell_command_set(_cmd);
			}
		} else if (keyboard_is_ctrl_pressed() && (c == 'c')) {
			// However, the ISR of the keyboard has already put the char.
			// Thus, delete it by using backspace.
			putchar('\b');
			// Re-set the index to the beginning.
			cmd_cursor_index = 0;
			// Go to the new line.
			printf("\n\n");
			// Sets the command.
			shell_command_set("\0");

			// Break the while loop.
			break;
		} else if (c == '\b') {
			shell_command_erase_char();
		} else {
			if (!shell_command_append_char(c)) {
				putchar('\b');
			}
		}
	} while (cmd_cursor_index < CMD_LEN);

	// Cleans all blanks at the beginning of the command.
	trim(cmd);
}

/// @brief Gets the inserted command.
static CommandFunction shell_find_command(char *command)
{
	if (command == NULL) {
		return NULL;
	}
	// Matching and executing the command.
	for (size_t it = 0; it < MAX_NUM_COM; ++it) {
		// Skip commands with undefined functions.
		if (_shell_commands[it].function == NULL) {
			continue;
		}
		if (strcmp(command, _shell_commands[it].cmdname) != 0) {
			continue;
		}

		return _shell_commands[it].function;
	}

	return NULL;
}

static inline bool_t shell_is_separator(char c)
{
	return ((c == '\0') || (c == ' ') || (c == '\t') || (c == '\n') ||
			(c == '\r'));
}

static int shell_count_words(const char *sentence)
{
	int result = 0;
	bool_t inword = false;
	const char *it = sentence;
	do
		if (shell_is_separator(*it)) {
			if (inword) {
				inword = false;
				result++;
			}
		} else {
			inword = true;
		}
	while (*it++);

	return result;
}

/// @brief Gets the options from the command.
/// @param command The executed command.
static void shell_get_options(char *command, int *argc, char ***argv)
{
	// Get the number of arguments, return if zero.
	if (((*argc) = shell_count_words(command)) == 0) {
		return;
	}
	(*argv) = (char **)malloc(sizeof(char *) * ((*argc) + 1));
	bool_t inword = false;
	const char *cit = command;
	size_t argcIt = 0, argIt = 0;
	do {
		if (shell_is_separator(*cit)) {
			if (inword) {
				inword = false;
				(*argv)[argcIt++][argIt] = '\0';
				argIt = 0;
			}
		} else {
			// Allocate string for argument.
			if (!inword) {
				(*argv)[argcIt] = (char *)malloc(sizeof(char) * CMD_LEN);
			}
			inword = true;
			(*argv)[argcIt][argIt++] = (*cit);
		}
	} while (*cit++);
	(*argv)[argcIt] = NULL;
}

void cmd_help(int argc, char **argv)
{
	if (argc > 2) {
		printf("Too many arguments.\n\n");
		return;
	}
	if (argc == 1) {
		printf("Available commands:\n");
		for (int i = 0, j = 0; i < MAX_NUM_COM; ++i) {
			if (_shell_commands[i].function != NULL) {
				printf("%-10s ", _shell_commands[i].cmdname);
				if ((j++) == 3) {
					printf("\n");
					j = 0;
				}
			}
		}
		printf("\n\n");
		return;
	}
	if (argc == 2) {
		for (int i = 0; i < MAX_NUM_COM; ++i) {
			if (strcmp(_shell_commands[i].cmdname, argv[1]) == 0) {
				printf("%s\n\n", _shell_commands[i].cmddesc);

				return;
			}
		}
		printf("Cannot find command: '%s'\n\n", argv[1]);
	}
	printf("\n");
}

int shell(int argc, char **argv, char **envp)
{
	dbg_print("I'm shell, I am the knight here...\n");

	video_set_color(BRIGHT_BLUE);
	printf("\t\t.: Welcome to MentOS :.\n\n");
	video_set_color(WHITE);

	dbg_print("I'm shell, I'll let my pawn, login, handle any intruder...\n");
	shell_login();

	sys_chdir("/");
	current_user.uid = 1;
	current_user.gid = 0;

	for (int i = 0; i < 50; ++i) {
		putchar('\n');
	}
	cmd_logo(1, NULL);
	printf("\n\n\n\n");

	dbg_print("I'm shell, let us begin...\n");

	while (true) {
		// First print the prompt.
		shell_print_prompt();
		// Get the input command.
		shell_get_command();
		// Check if the command is empty.
		if (strlen(cmd) <= 0) {
			continue;
		}
		// Retrieve the options from the command.
		/// The current number of arguments.
		int _argc = 1;
		/// The vector of arguments.
		char **_argv;
		shell_get_options(cmd, &_argc, &_argv);
		// Check if the command is empty.
		if (_argc == 0) {
			continue;
		}
		// Add the command to the history.
		history_push(cmd);
		// Find the command.
		CommandFunction commandFunction = shell_find_command(_argv[0]);
		if (commandFunction == NULL) {
			printf("\nUnknown command: %s\n", _argv[0]);
		} else if (strcmp(_argv[0], "cd") == 0) {
			commandFunction(_argc, _argv);
		} else {
			int status;
			pid_t cpid = vfork();
			if (cpid == 0) {
				char *_envp[] = { (char *)NULL };
				execve((const char *)commandFunction, _argv, _envp);
				kernel_panic("This is bad, I should not be here!\n");
			}
			waitpid(cpid, &status, 0);
		}

		// Free up the memory reserved for the arguments.
		for (int it = 0; it < _argc; ++it) {
			// Check if the argument is not empty.
			if (_argv[it] != NULL) {
				// Free up its memory.
				free(_argv[it]);
			}
		}
		free(_argv);
	}

	return 0;
}

void move_cursor_left(void)
{
	if (cmd_cursor_index > lower_bound_x) {
		--cmd_cursor_index;
	}
}

void move_cursor_right(void)
{
	if (cmd_cursor_index < shell_lower_bound_x) {
		++cmd_cursor_index;
	}
}
