/// @file ipc.h
/// @brief Inter-Process Communication (IPC) structures.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/types.h"
#include "stddef.h"

#define IPC_CREAT  01000 ///< Create key if key does not exist.
#define IPC_EXCL   02000 ///< Fail if key exists.
#define IPC_NOWAIT 04000 ///< Return error on wait.
#define IPC_RMID   0     ///< Remove identifier.
#define IPC_SET    1     ///< Set `ipc_perm' options.
#define IPC_STAT   2     ///< Get `ipc_perm' options.
#define IPC_INFO   3     ///< See ipcs.

#define IPC_PRIVATE ((key_t)0) ///< assures getting a new ipc_key.

/// @brief Permission details of an IPC object.
struct ipc_perm {
    /// Key associated to the IPC.
    key_t key;
    /// Effective UID of owner.
    uid_t uid;
    /// Effective GID of owner.
    gid_t gid;
    /// Effective UID of creator.
    uid_t cuid;
    /// Effective GID of creator.
    gid_t cgid;
    /// Permissions.
    unsigned short mode;
    /// Sequence number.
    unsigned short __seq;
};

/// @brief Returns a possible IPC key based upon the filepath and the id.
/// @param path The file path.
/// @param id the project id.
/// @return the IPC key.
key_t ftok(char *path, int id);

/// @brief Prints System V IPC Informations
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long ipcs();