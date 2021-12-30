/// @file sem.h
/// @brief Definition of structure for managing semaphores.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
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

long sys_semget(key_t key, int nsems, int semflg);

long sys_semop(int semid, struct sembuf *sops, unsigned nsops);

long sys_semctl(int semid, int semnum, int cmd, unsigned long arg);

#else

long semget(key_t key, int nsems, int semflg);

long semop(int semid, struct sembuf *sops, unsigned nsops);

long semctl(int semid, int semnum, int cmd, unsigned long arg);

#endif