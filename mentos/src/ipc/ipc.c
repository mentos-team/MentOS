/// @file ipc.c
/// @brief Vital IPC structures and functions kernel side.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "ipc/ipc.h"

#include "process/scheduler.h"
#include "io/debug.h"
#include "assert.h"
#include "fcntl.h"

int ipc_check_perm(
    struct ipc_perm *perm,
    int usr,
    int grp,
    int oth)
{
    // Get the calling task.
    task_struct *task = scheduler_get_current_process();
    assert(task && "Failed to get the current running process.");
    assert(perm && "Received a NULL ipc_perm.");
    // Init process has unlimited power.
    if (task->pid == 0)
        return 1;
    int check_parent = (perm->key < 0) && task->parent && (task->parent->pid != 0);
    if ((perm->mode & usr)) {
        if ((perm->uid == task->uid) || (perm->cuid == task->uid))
            return 1;
        if (check_parent && ((perm->uid == task->parent->uid) || (perm->cuid == task->parent->uid)))
            return 1;
    }
    if ((perm->mode & grp)) {
        if ((perm->gid == task->gid) || (perm->cgid == task->gid))
            return 1;
        if (check_parent && ((perm->gid == task->parent->gid) || (perm->cgid == task->parent->gid)))
            return 1;
    }
    if ((perm->mode & oth)) {
        if (check_parent && ((perm->uid == task->parent->uid) || (perm->cuid == task->parent->uid)))
            return 1;
        if (check_parent && ((perm->gid == task->parent->gid) || (perm->cgid == task->parent->gid)))
            return 1;
    }
    return 0;
}

int ipc_valid_permissions(int flags, struct ipc_perm *perm)
{
    // If the key is negative, it means it was a private key.
    if ((flags & O_RDONLY) && ipc_check_perm(perm, S_IRUSR, S_IRGRP, S_IROTH))
        return 1;
    if ((flags & O_WRONLY) && ipc_check_perm(perm, S_IWUSR, S_IWGRP, S_IWOTH))
        return 1;
    if ((flags & O_RDWR) && ipc_check_perm(perm, S_IRUSR | S_IWUSR, S_IRGRP | S_IWGRP, S_IROTH | S_IWOTH))
        return 1;
    return 0;
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