///                MentOS, The Mentoring Operating system project
/// @file shell.h
/// @brief Data structure used to implement the Shell.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"
#include "kernel.h"

/// Maximum length of credentials.
#define CREDENTIALS_LENGTH  50

/// Maximum length of commands.
#define CMD_LEN             256

/// Maximum length of descriptions.
#define DESC_LEN            256

/// Maximum number of saved commands.
#define MAX_NUM_COM         50

/// Maximum length of history.
#define HST_LEN             10

#define KEY_UP    72

#define KEY_DOWN  80

#define KEY_LEFT  75

#define KEY_RIGHT 77

/// Pointer to the function of a commmand.
typedef void (* CommandFunction)(int argc, char **argv);

/// @brief Holds information about a command.
typedef struct command_t
{
    /// The name of the command.
    char cmdname[CMD_LEN];

    /// The function pointer to the command.
    CommandFunction function;

    /// The description of the command.
    char cmddesc[DESC_LEN];
} command_t;

/// @brief Holds information about the user.
typedef struct userenv_t
{
    /// The username.
    char username[CREDENTIALS_LENGTH];

    /// The current path.
    char cur_path[MAX_PATH_LENGTH];

    /// The user identifier.
    unsigned int uid;

    /// The group identifier.
    unsigned int gid;
} userenv_t;

/// Contains the information about the current user.
extern userenv_t current_user;

/// @brief The shell.
int shell(int argc, char **argv, char **envp);

/// @brief Moves the cursor left.
void move_cursor_left(void);

/// @brief Moves the cursor right.
void move_cursor_right(void);
