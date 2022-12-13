/// @file ipc.h
/// @brief Inter-Process Communication (IPC) structures.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Permission details of an IPC object.
struct ipc_perm {
    /// Key supplied to msgget(2).
    key_t __key;
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
