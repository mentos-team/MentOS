/// @file ipc.c
/// @brief Vital IPC structures and functions kernel side.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "ipc/ipc.h"

#include "assert.h"
#include "io/debug.h"
#include "process/scheduler.h"
#include "sys/stat.h"

/// @brief Checks IPC permissions for a task.
/// @param task Pointer to the task structure.
/// @param perm Pointer to the IPC permissions structure.
/// @param usr Permission flag for user.
/// @param grp Permission flag for group.
/// @param oth Permission flag for others.
/// @return 1 if permissions are granted, 0 otherwise.
static inline int ipc_check_perm(const task_struct *task, const struct ipc_perm *perm, unsigned short usr,
                                 unsigned short grp, unsigned short oth)
{
    // Check if task is NULL.
    if (!task) {
        pr_err("Received a NULL task.\n");
        return 0;
    }
    // Check if perm is NULL.
    if (!perm) {
        pr_err("Received a NULL perm.\n");
        return 0;
    }
    // As with regular Unix permission checks, exactly one class applies: an
    // owner that lacks a requested bit must not fall through to "others".
    if (perm->uid == task->uid) {
        return (perm->mode & usr) != 0;
    }
    if (perm->gid == task->gid) {
        return (perm->mode & grp) != 0;
    }
    return (perm->mode & oth) != 0;
}

int ipc_valid_permissions(unsigned int access, const struct ipc_perm *perm)
{
    // Get the calling task.
    task_struct *task = scheduler_get_current_process();
    assert(task && "Failed to get the current running process.");
    // Init and uid 0 are superuser contexts.  Membership in gid 0 must still
    // obey the object's group permission bits; it is not equivalent to root.
    if ((task->pid == 0) || (task->uid == 0)) {
        return 1;
    }
    if ((access & IPC_ACCESS_READ) && !ipc_check_perm(task, perm, S_IRUSR, S_IRGRP, S_IROTH)) {
        return 0;
    }
    if ((access & IPC_ACCESS_WRITE) && !ipc_check_perm(task, perm, S_IWUSR, S_IWGRP, S_IWOTH)) {
        return 0;
    }
    return 1;
}

int ipc_valid_mode(mode_t mode, const struct ipc_perm *perm)
{
    unsigned int access = 0;
    mode &= (S_IRWXU | S_IRWXG | S_IRWXO);

    if (mode & (S_IRUSR | S_IRGRP | S_IROTH)) {
        access |= IPC_ACCESS_READ;
    }
    if (mode & (S_IWUSR | S_IWGRP | S_IWOTH)) {
        access |= IPC_ACCESS_WRITE;
    }
    return ipc_valid_permissions(access, perm);
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
