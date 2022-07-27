/// @file pwd.h
/// @brief Contains the structure and functions for managing passwords.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// @brief Stores user account information.
typedef struct passwd_t {
    char *pw_name;   ///< User's login name.
    char *pw_passwd; ///< Encrypted password (not currently).
    uid_t pw_uid;    ///< User ID.
    gid_t pw_gid;    ///< Group ID.
    char *pw_gecos;  ///< User's full name.
    char *pw_dir;    ///< User's login directory.
    char *pw_shell;  ///< User's login shell.
} passwd_t;

/// @brief Provides access to the password database entry for `name`.
/// @param name The name to search inside the database.
/// @return A pointer to the structure containing the database entry.
passwd_t *getpwnam(const char *name);

/// @brief Provides access to the password database entry for `uid`.
/// @param uid The uid to search inside the database.
/// @return A pointer to the structure containing the database entry.
passwd_t *getpwuid(uid_t uid);

/// @brief Provides the same information as getpwnam but it stores the
///        results inside pwd, and the string information are store store
///        inside `buf`.
/// @param name   The name to search inside the database.
/// @param pwd    The structure containing pointers to the entry fields.
/// @param buf    The buffer where the strings should be stored.
/// @param buflen The lenght of the buffer.
/// @param result A pointer to the result or NULL is stored here.
/// @return If the entry was found returns zero and set *result to pwd,
///         if the entry was not found returns zero and set *result to NULL,
///         on failure returns a number and sets  and set *result to NULL.
int getpwnam_r(const char *name, passwd_t *pwd, char *buf, size_t buflen, passwd_t **result);

/// @brief Provides the same information as getpwuid but it stores the
///        results inside pwd, and the string information are store store
///        inside `buf`.
/// @param uid    The uid to search inside the database.
/// @param pwd    The structure containing pointers to the entry fields.
/// @param buf    The buffer where the strings should be stored.
/// @param buflen The lenght of the buffer.
/// @param result A pointer to the result or NULL is stored here.
/// @return If the entry was found returns zero and set *result to pwd,
///         if the entry was not found returns zero and set *result to NULL,
///         on failure returns a number and sets  and set *result to NULL.
int getpwuid_r(uid_t uid, passwd_t *pwd, char *buf, size_t buflen, passwd_t **result);
