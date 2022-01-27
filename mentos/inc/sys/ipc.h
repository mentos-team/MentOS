/// @file ipc.h
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

#define IPC_CREAT  01000 ///< Create key if key does not exist.
#define IPC_EXCL   02000 ///< Fail if key exists.
#define IPC_NOWAIT 04000 ///< Return error on wait.
#define IPC_RMID   0     ///< Remove identifier.
#define IPC_SET    1     ///< Set `ipc_perm' options.
#define IPC_STAT   2     ///< Get `ipc_perm' options.
#define IPC_INFO   3     ///< See ipcs.

/// @brief Holds details about IPCs.
typedef struct ipc_perm_t {
    /// Creator user id.
    uid_t cuid;
    /// Creator group id.
    gid_t cgid;
    /// User id.
    uid_t uid;
    /// Group id.
    gid_t gid;
    /// r/w permission.
    ushort mode;
    /// Sequence # (to generate unique msg/sem/shm id).
    ushort seq;
    /// User specified msg/sem/shm key.
    key_t key;
} ipc_perm_t;
