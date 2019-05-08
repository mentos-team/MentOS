///                MentOS, The Mentoring Operating system project
/// @file commands.h
/// @brief Prototypes of functions for the Shell.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "shell.h"

/// @brief Prints the logo.
void cmd_logo(int argc, char **argv);

/// @brief Returns the list of commands.
void cmd_help(int argc, char **argv);

/// @brief Echos the given message.
void cmd_echo(int argc, char **argv);

/// @brief Power off the machine.
void cmd_poweroff(int argc, char **argv);

/// @brief Call the uname function.
void cmd_uname(int argc, char **argv);

/// @brief Prints the credits.
void cmd_credits(int argc, char **argv);

/// @brief Sleeps the thread of the shell for a given period of time.
void cmd_sleep(int argc, char **argv);

/// @brief Shows the output of the cpuid function.
void cmd_cpuid(int argc, char **argv);

/// @brief Lists the directory.
void cmd_ls(int argc, char **argv);

/// @brief Move to another directory.
void cmd_cd(int argc, char **argv);

/// @brief Creates a new directory.
void cmd_mkdir(int argc, char **argv);

/// @brief Removes a file.
void cmd_rm(int argc, char **argv);

/// @brief Removes a directory.
void cmd_rmdir(int argc, char **argv);

/// @brief Show the current user name.
void cmd_whoami(int argc, char **argv);

/// @brief Allows to test some functionalities of the kernel.
void cmd_tester(int argc, char **argv);

/// @brief Print current working directory.
void cmd_pwd(int argc, char **argv);

/// @brief Read content of a file.
void cmd_more(int argc, char **argv);

/// @brief Create a new file.
void cmd_touch(int argc, char **argv);

/// @brief Create a new file.
void cmd_newfile(int argc, char **argv);

/// @brief Show task list.
void cmd_ps(int argc, char **argv);

/// @brief Show date and time.
void cmd_date(int argc, char **argv);

/// @brief Clears the screen.
void cmd_clear(int argc, char **argv);

/// @brief Shows the PID of the shell.
void cmd_showpid(int argc, char **argv);

/// @brief Prints the history.
void cmd_show_history(int argc, char **argv);

/// @brief Loads the drivers.
void cmd_drv_load(int argc, char **argv);

/// @brief Show IPC state.
void cmd_ipcs(int argc, char **argv);

/// @brief Remove IPC resorce from id.
void cmd_ipcrm(int argc, char **argv);

/// @brief Change the nice value.
void cmd_nice(int argc, char **argv);
