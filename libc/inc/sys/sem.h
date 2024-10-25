/// @file sem.h
/// @brief Definition of structure for managing semaphores.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/types.h"
#include "stddef.h"
#include "sys/ipc.h"
#include "time.h"

#define SEM_UNDO 0x1000 ///< Undo the operation on exit.

/// @defgroup SemaphoreCommands semctl commands
/// @brief List of commands for semctl function.
/// @{

#define GETPID   11 ///< Get sempid.
#define GETVAL   12 ///< Get semval.
#define GETALL   13 ///< Get all semval's.
#define GETNCNT  14 ///< Get semncnt.
#define GETZCNT  15 ///< Get semzcnt.
#define SETVAL   16 ///< Set semval.
#define SETALL   17 ///< Set all semval's.
#define SEM_STAT 18 ///< Return a semid_ds structure.
#define SEM_INFO 19 ///< Return a seminfo structure.

/// }@

#define SEM_SET_MAX 256

/// @brief Optional argument for semctl() function
union semun {
    /// @brief Value for SETVAL.
    int val;
    /// @brief Buffer for IPC_STAT & IPC_SET.
    struct semid_ds *buf;
    /// @brief Array for GETALL & SETALL.
    unsigned short *array;
    /// @brief Buffer for IPC_INFO.
    struct seminfo *__buf;
};

/// @brief Single Semaphore.
struct sem {
    /// @brief Process ID of the last operation.
    pid_t sem_pid;
    /// @brief Semaphore Value.
    unsigned short sem_val;
    /// @brief Number of processes waiting for the semaphore.
    unsigned short sem_ncnt;
    /// @brief Number of processes waiting for the value to become 0.
    unsigned short sem_zcnt;
};

/// @brief Semaphore set
struct semid_ds {
    /// @brief Ownership and permissions.
    struct ipc_perm sem_perm;
    /// @brief Last semop time.
    time_t sem_otime;
    /// @brief Last change time.
    time_t sem_ctime;
    /// @brief Number of semaphores in set.
    unsigned short sem_nsems;
};

/// @brief Buffer to use with the semaphore IPC.
struct sembuf {
    /// @brief Semaphore index in array.
    unsigned short sem_num;
    /// @brief Semaphore operation.
    short sem_op;
    /// @brief Operation flags.
    short sem_flg;
};

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
long semctl(int semid, int semnum, int cmd, union semun *arg);
