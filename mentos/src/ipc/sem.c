/// @file sem.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///! @cond Doxygen_Suppress

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[IPCsem]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.
#include <stdio.h> //test
#include "ipc/sem.h" //test
#include "system/panic.h"


//test
long sys_semget(key_t key, int nsems, int semflg)
{
    /*PSEUDOCODE (todo)*/


    /*
    Check if num_sems is a valid value
    */
    /* .... */


    /*
    Compute the IPC id using key. (Hashing Algorithm?)
    */
    /* .... */


    /*
    Check if a semaphore set with the ID already exists (and then check for IPC_EXCL flag)
    */
    /* .... */

    /*
    If the semaphore does not exist, create a new semaphore set with the specified number of sems
    (allocation, initialization...) 
    */
    /* .... */

    /*
    Set the semaphore set permissions (default 0666)
    */
    /* .... */


    /*
    Return the new semaphore set ID
    */
    /* .... */





    //printf("qui");  test
    //TODO("Not implemented");
    return 0;
}

long sys_semop(int semid, struct sembuf *sops, unsigned nsops)
{
    /*
    Check if semid is a valid semaphore set identifier
    */
    /* .... */


    /*
    Handle the sembuf structure and perform the operation (spinlocks?)
    */
    /* .... */

    /*
    Update the semaphores values
    */
    /* .... */


    //TODO("Not implemented");
    return 0;
}

long sys_semctl(int semid, int semnum, int cmd, unsigned long arg)
{

    /*
    Check if semid is a valid semaphore identifier
    */
    /* .... */

    /*
    Check the cmd parameter (GETVAL, SETVAL...) and perform the operation
    */
    /* .... */


    //TODO("Not implemented");
    return 0;
}

///! @endcond
