/// @file shadow.h
/// @brief Defines structures and functions for working with the shadow password
/// file.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

#define SHADOW "/etc/shadow" ///< Path to the shadow password file.

/// @brief Structure representing a shadow password record.
/// @details This structure is used to store details from the shadow password
/// file (`/etc/shadow`), including information such as the user's encrypted
/// password and password change policies.
struct spwd {
    char *sp_namp;             ///< User login name.
    char *sp_pwdp;             ///< Encrypted password.
    long int sp_lstchg;        ///< Date of the last password change, in days since the epoch.
    long int sp_min;           ///< Minimum number of days until the next password change is allowed.
    long int sp_max;           ///< Maximum number of days before a password change is required.
    long int sp_warn;          ///< Number of days before the password expires to warn the user.
    long int sp_inact;         ///< Number of days after expiration until the account is considered inactive.
    long int sp_expire;        ///< Date when the account expires, in days since the epoch.
    unsigned long int sp_flag; ///< Reserved for future use.
};

/// @brief Retrieves a shadow password record by username.
///
/// @details This function retrieves the shadow password entry for a specific
/// user from the shadow password file (`/etc/shadow`). It uses a static buffer
/// to store the result, which is overwritten on each call.
///
/// @param name The login name of the user to search for.
/// @return Pointer to the `spwd` structure with the user's shadow password entry, or NULL if not found.
struct spwd *getspnam(const char * name);

/// @brief Retrieves a shadow password record by username (reentrant version).
///
/// @details This function retrieves the shadow password entry for a specific
/// user in a reentrant manner. It stores the result in user-provided buffers to
/// avoid race conditions. This is the safer, thread-safe version of
/// `getspnam()`.
///
/// @param name The login name of the user to search for.
/// @param spwd_buf Pointer to a user-provided `spwd` structure where the result will be stored.
/// @param buf Buffer to hold additional string data like the encrypted password.
/// @param buflen Size of the buffer provided.
/// @param result Pointer to the result. On success, this will point to `spwd_buf`, or NULL on failure.
/// @return 0 on success, or a non-zero error code on failure.
int getspnam_r(const char *name, struct spwd *spwd_buf, char *buf, size_t buflen, struct spwd **result);
