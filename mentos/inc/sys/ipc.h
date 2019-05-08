///                MentOS, The Mentoring Operating system project
/// @file ipc.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// Create key if key does not exist.
#define IPC_CREAT	01000

/// Fail if key exists.
#define IPC_EXCL	02000

/// Return error on wait.
#define IPC_NOWAIT	04000

/// Remove identifier.
#define IPC_RMID	0

/// Set `ipc_perm' options.
#define IPC_SET		1

/// Get `ipc_perm' options.
#define IPC_STAT	2

/// See ipcs.
#define IPC_INFO	3

struct ipc_perm {
    /// Creator user id.
    uid_t	cuid;

    /// Creator group id.
    gid_t	cgid;

    /// User id.
    uid_t	uid;

    /// Group id.
    gid_t	gid;

    /// r/w permission.
    ushort	mode;

    /// Sequence # (to generate unique msg/sem/shm id).
    ushort	seq;

    /// User specified msg/sem/shm key.
    key_t	key;
};
