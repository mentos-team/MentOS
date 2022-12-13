/// @file sem.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///! @cond Doxygen_Suppress

#include "ipc/sem.h"
#include "system/panic.h"

long sys_semget(key_t key, int nsems, int semflg)
{
    TODO("Not implemented");
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
