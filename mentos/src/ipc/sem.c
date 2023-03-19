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
    printf("qui");
    //TODO("Not implemented");
    return 0;
}

long sys_semop(int semid, struct sembuf *sops, unsigned nsops)
{
    TODO("Not implemented");
    return 0;
}

long sys_semctl(int semid, int semnum, int cmd, unsigned long arg)
{
    TODO("Not implemented");
    return 0;
}

///! @endcond
