/// @file ipc.c
/// @brief Vital IPC structures and functions kernel side.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "ipc/ipc.h"

#include "process/scheduler.h"
#include "io/debug.h"
#include "assert.h"
#include "fcntl.h"

int ipc_valid_permissions(int flags, struct ipc_perm *perm)
{
    // Get the calling task.
    task_struct *task = scheduler_get_current_process();
    assert(task && "Failed to get the current running process.");
    // If the key is negative, it means it was a private key.
    if (perm->key < 0) {
        do {
            if ((perm->uid == task->uid) || (perm->cuid == task->uid)) {
                if (bitmask_check(flags, O_RDONLY) && bitmask_check(perm->mode, S_IRUSR))
                    return 1;
                if (bitmask_check(flags, O_WRONLY) && bitmask_check(perm->mode, S_IWUSR))
                    return 1;
                if (bitmask_check(flags, O_RDWR) && bitmask_check(perm->mode, S_IRUSR) && bitmask_check(perm->mode, S_IWUSR))
                    return 1;
            }
            task = task->parent;
        } while (task && task->uid);
        return 0;
    }
    // Check if the process itself has permissions.
    if ((perm->uid == task->uid) || (perm->cuid == task->uid)) {
        if (bitmask_check(flags, O_RDONLY) && !bitmask_check(perm->mode, S_IRUSR))
            return 0;
        if (bitmask_check(flags, O_WRONLY) && !bitmask_check(perm->mode, S_IWUSR))
            return 0;
        if (bitmask_check(flags, O_RDWR) && (!bitmask_check(perm->mode, S_IRUSR) || !bitmask_check(perm->mode, S_IWUSR)))
            return 0;
    }
    if ((perm->gid == task->gid) || (perm->cgid == task->gid)) {
        if (bitmask_check(flags, O_RDONLY) && !bitmask_check(perm->mode, S_IRGRP))
            return 0;
        if (bitmask_check(flags, O_WRONLY) && !bitmask_check(perm->mode, S_IWGRP))
            return 0;
        if (bitmask_check(flags, O_RDWR) && (!bitmask_check(perm->mode, S_IRGRP) || !bitmask_check(perm->mode, S_IWGRP)))
            return 0;
    }
    if ((perm->uid != task->uid) && (perm->cuid != task->uid) && (perm->gid != task->gid) && (perm->cgid != task->gid)) {
        if (bitmask_check(flags, O_RDONLY) && !bitmask_check(perm->mode, S_IROTH))
            return 0;
        if (bitmask_check(flags, O_WRONLY) && !bitmask_check(perm->mode, S_IWOTH))
            return 0;
        if (bitmask_check(flags, O_RDWR) && (!bitmask_check(perm->mode, S_IROTH) || !bitmask_check(perm->mode, S_IWOTH)))
            return 0;
    }
    return 1;
}

struct ipc_perm register_ipc(key_t key, mode_t mode)
{
    struct ipc_perm ip;
    // Get the current task.
    task_struct *task = scheduler_get_current_process();
    assert(task && "Failed to get the current running process.");
    // Initialize the structure.
    ip.key   = key;
    ip.uid   = task->uid;
    ip.gid   = task->gid;
    ip.cuid  = task->uid;
    ip.cgid  = task->gid;
    ip.mode  = mode;
    ip.__seq = 0;
    return ip;
}