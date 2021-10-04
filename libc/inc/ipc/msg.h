///                MentOS, The Mentoring Operating system project
/// @file msg.h
/// @brief Definition of structure for managing message queues.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/types.h"
#include "stddef.h"
#include "time.h"
#include "ipc.h"

/// Type for storing the number of messages in a message queue.
typedef unsigned int msgqnum_t;

/// Type for storing the number of bytes in a message queue.
typedef unsigned int msglen_t;

/// @brief Buffer to use with the message queue IPC.
struct msgbuf {
    /// Type of the message.
    long mtype;
    /// Text of the message.
    char mtext[1];
};

/// @brief Message queue data structure.
struct msqid_ds {
    /// Ownership and permissions.
    struct ipc_perm msg_perm;
    /// Time of last msgsnd(2).
    time_t msg_stime;
    /// Time of last msgrcv(2).
    time_t msg_rtime;
    /// Time of creation or last modification by msgctl().
    time_t msg_ctime;
    /// Number of bytes in queue.
    unsigned long msg_cbytes;
    /// Number of messages in queue.
    msgqnum_t msg_qnum;
    /// Maximum number of bytes in queue.
    msglen_t msg_qbytes;
    /// PID of last msgsnd(2).
    pid_t msg_lspid;
    /// PID of last msgrcv(2).
    pid_t msg_lrpid;
};

#ifdef __KERNEL__

long sys_msgget(key_t key, int msgflg);

long sys_msgsnd(int msqid, struct msgbuf *msgp, size_t msgsz, int msgflg);

long sys_msgrcv(int msqid, struct msgbuf *msgp, size_t msgsz, long msgtyp, int msgflg);

long sys_msgctl(int msqid, int cmd, struct msqid_ds *buf);

#else

long msgget(key_t key, int msgflg);

long msgsnd(int msqid, struct msgbuf *msgp, size_t msgsz, int msgflg);

long msgrcv(int msqid, struct msgbuf *msgp, size_t msgsz, long msgtyp, int msgflg);

long msgctl(int msqid, int cmd, struct msqid_ds *buf);

#endif