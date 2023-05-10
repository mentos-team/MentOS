/// @file ipc.c
/// @brief Vital IPC structures and functions kernel side.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "ipc/ipc.h"

#include "process/scheduler.h"
#include "assert.h"

struct ipc_perm register_ipc(key_t key)
{
    struct ipc_perm ip;
    // Get the current task.
    task_struct *task = scheduler_get_current_process();
    // Check the running task.
    assert(task && "There is no running process!");
    // Initialize the structure.
    if (key == IPC_PRIVATE) {
        // Generate a unique key.
        ip.__key = key;
    } else {
        ip.__key = key;
    }
    ip.uid   = task->uid;
    ip.gid   = task->gid;
    ip.cuid  = task->uid;
    ip.cgid  = task->gid;
    ip.mode  = 0;
    ip.__seq = 0;
    return ip;
}