/// @file ipc.h
/// @brief Vital IPC structures and functions kernel side.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/ipc.h"

#ifndef __KERNEL__
#error "How did you include this file... include `libc/inc/sys/ipc.h` instead!"
#endif

int ipc_valid_permissions(int flags, struct ipc_perm *perm);

struct ipc_perm register_ipc(key_t key, mode_t mode);

/// @brief Initializes the semaphore system.
/// @return 0 on success, 1 on failure.
int sem_init(void);

/// @brief Initializes the shared memory.
/// @return 0 on success, 1 on failure.
int shm_init(void);

/// @brief Initializes the message queue system.
/// @return 0 on success, 1 on failure.
int msq_init(void);
