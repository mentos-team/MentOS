/// @file grp.h
/// @brief Defines the structures and functions for managing groups.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// Maximum number of users per group
#define MAX_MEMBERS_PER_GROUP 64

/// @brief Contains user group informations.
typedef struct group {
    /// The name of the group.
    char *gr_name;
    /// Encrypted password.
    char *gr_passwd;
    /// Group ID.
    gid_t gr_gid;
    /// List of group members.
    char *gr_mem[MAX_MEMBERS_PER_GROUP + 1];
} group_t;

/// @brief Provides access to the group database entry for `uid`.
/// @param gid The gid to search inside the database.
/// @return A pointer to the structure containing the database entry.
group_t *getgrgid(gid_t gid);

/// @brief Provides access to the group database entry for `name`.
/// @param name The name to search inside the database.
/// @return A pointer to the structure containing the database entry.
group_t *getgrnam(const char *name);

/// @brief Provides the same information as getgrgid but it stores the results
/// inside group, and the string information are store store inside `buf`.
/// @param gid The uid to search inside the database.
/// @param group The structure containing pointers to the entry fields.
/// @param buf The buffer where the strings should be stored.
/// @param buflen The lenght of the buffer.
/// @param result A pointer to the result or NULL is stored here.
/// @return If the entry was found returns zero and set *result to group, if the
/// entry was not found returns zero and set *result to NULL, on failure returns
/// a number and sets  and set *result to NULL.
int getgrgid_r(gid_t gid, group_t *group, char *buf, size_t buflen, group_t **result);

/// @brief Provides the same information as getgrnam but it stores the results
///inside group, and the string information are store store inside `buf`.
/// @param name The name to search inside the database.
/// @param group The structure containing pointers to the entry fields.
/// @param buf The buffer where the strings should be stored.
/// @param buflen The lenght of the buffer.
/// @param result A pointer to the result or NULL is stored here.
/// @return If the entry was found returns zero and set *result to group, if the
/// entry was not found returns zero and set *result to NULL, on failure returns
/// a number and sets  and set *result to NULL.
int getgrnam_r(const char *name, group_t *group, char *buf, size_t buflen, group_t **result);

/// @brief Returns a pointer to a structure containing the broken-out fields of
/// an entry in the group database.
/// @return pointer to the group entry.
/// @details When first called returns a pointer to a group structure containing
/// the first entry in the group database. Thereafter, it returns a pointer to a
/// group structure containing the next group structure in the group database,
/// so successive calls may be used to search the entire database.
group_t *getgrent(void);

/// @brief Rewinds the group database to allow repeated searches.
void endgrent(void);

/// @brief May be called to close the group database when processing is complete.
void setgrent(void);
