/// @file sem.h
/// @brief Definition of structure for managing semaphores.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/types.h"
#include "stddef.h"
#include "time.h"
#include "ipc.h"

/// @brief Buffer to use with the semaphore IPC.
struct sembuf {
    /// Semaphore index in array.
    unsigned short sem_num;
    /// Semaphore operation.
    short sem_op;
    /// Operation flags.
    short sem_flg;
};

#ifdef __KERNEL__

/// @brief Get a System V semaphore set identifier.
/// @param key can be used either to obtain the identifier of a previously
/// created semaphore set, or to create a new set.
/// @param nsems number of semaphores.
/// @param semflg controls the behaviour of the function.
/// @return the semaphore set identifier, -1 on failure, and errno is set to
/// indicate the error.
long sys_semget(key_t key, int nsems, int semflg);

/// @brief Performs operations on selected semaphores in the set.
/// @param semid the semaphore set identifier.
/// @param sops specifies operations to be performed on single semaphores.
/// @param nsops number of operations.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long sys_semop(int semid, struct sembuf *sops, unsigned nsops);

/// @brief Performs control operations on a semaphore set.
/// @param semid the semaphore set identifier.
/// @param semnum the n-th semaphore of the set on which we perform the operations.
/// @param cmd the command to perform.
/// @param arg
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long sys_semctl(int semid, int semnum, int cmd, unsigned long arg);

#else

/// @brief Get a System V semaphore set identifier.
/// @param key can be used either to obtain the identifier of a previously
/// created semaphore set, or to create a new set.
/// @param nsems number of semaphores.
/// @param semflg controls the behaviour of the function.
/// @return the semaphore set identifier, -1 on failure, and errno is set to
/// indicate the error.
long semget(key_t key, int nsems, int semflg);

/// @brief Performs operations on selected semaphores in the set.
/// @param semid the semaphore set identifier.
/// @param sops specifies operations to be performed on single semaphores.
/// @param nsops number of operations.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long semop(int semid, struct sembuf *sops, unsigned nsops);

/// @brief Performs control operations on a semaphore set.
/// @param semid the semaphore set identifier.
/// @param semnum the n-th semaphore of the set on which we perform the operations.
/// @param cmd the command to perform.
/// @param arg
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long semctl(int semid, int semnum, int cmd, unsigned long arg);

#endif
