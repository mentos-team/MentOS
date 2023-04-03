/// @file sem.h
/// @brief Definition of structure for managing semaphores.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/types.h"
#include "stddef.h"
#include "time.h"
#include "ipc.h"






/// @brief flags for semop
#define SEM_UNDO	0x1000		/* undo the operation on exit */


/// #brief Commands for semctl
#define GETPID		11		/* get sempid */
#define GETVAL		12		/* get semval */
#define GETALL		13		/* get all semval's */
#define GETNCNT		14		/* get semncnt */
#define GETZCNT		15		/* get semzcnt */
#define SETVAL		16		/* set semval */
#define SETALL		17		/* set all semval's */






/// @brief Optional argument for semctl() function
union semun {
    int val;                /* value for SETVAL */
    struct semid_ds *buf;   /* buffer for IPC_STAT & IPC_SET */
    unsigned short *array;  /* array for GETALL & SETALL */
    struct seminfo *__buf;  /* buffer for IPC_INFO */
};

/// @brief Single Semaphore
struct sem{
    unsigned short  sem_val; /*Semaphore Value*/
    pid_t           sem_pid; /*Process ID of the last operation*/   
    //unsigned short  semncnt; /*Number of processes waiting of semaphore*/   
    unsigned short  sem_zcnt; /*Number of processes waiting for the value to become 0*/
};

/// @brief Semaphore set
struct semid_ds {
    pid_t           owner;  /* Ownership and permissions */
    key_t           key; /*IPC_KEY associated to the semaphore set*/
    int             semid; /*semid associated to the semaphore set*/
    time_t          sem_otime; /* Last semop time */
    time_t          sem_ctime; /* Last change time */
    unsigned long   sem_nsems; /* No. of semaphores in set */
    struct sem      *sems; /*all the semaphores*/
};




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
