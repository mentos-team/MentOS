///                MentOS, The Mentoring Operating system project
/// @file msg.c
/// @brief
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///! @cond Doxygen_Suppress

#include "ipc/msg.h"
#include "panic.h"

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