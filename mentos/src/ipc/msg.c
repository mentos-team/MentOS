/// @file msg.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///! @cond Doxygen_Suppress

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[IPCmsg]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "ipc/msg.h"
#include "system/panic.h"

long sys_msgget(key_t key, int msgflg)
{
    TODO("Not implemented");
    return 0;
}

long sys_msgsnd(int msqid, struct msgbuf *msgp, size_t msgsz, int msgflg)
{
    TODO("Not implemented");
    return 0;
}

long sys_msgrcv(int msqid, struct msgbuf *msgp, size_t msgsz, long msgtyp, int msgflg)
{
    TODO("Not implemented");
    return 0;
}

long sys_msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
    TODO("Not implemented");
    return 0;
}

///! @endcond
