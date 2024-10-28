/// @file ipc.h
/// @brief Vital IPC structures and functions kernel side.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/ipc.h"

#ifndef __KERNEL__
#error "How did you include this file... include `libc/inc/sys/ipc.h` instead!"
#endif

/// @brief Validate IPC permissions based on flags and the given permission structure.
/// @param flags Flags that control the validation behavior.
/// @param perm Pointer to the IPC permission structure to validate.
/// @return 0 if the permissions are valid, or a non-zero value on failure.
int ipc_valid_permissions(int flags, struct ipc_perm *perm);

/// @brief Register an IPC resource with a given key and mode.
/// @param key The key associated with the IPC resource.
/// @param mode The mode (permissions) for the IPC resource.
/// @return A `struct ipc_perm` representing the registered IPC resource.
struct ipc_perm register_ipc(key_t key, mode_t mode);

/// @brief Initializes the semaphore system.
/// @return 0 on success, 1 on failure.
int sem_init(void);

/// @brief Initializes the shared memory system.
/// @return 0 on success, 1 on failure.
int shm_init(void);

/// @brief Initializes the message queue system.
/// @return 0 on success, 1 on failure.
int msq_init(void);
