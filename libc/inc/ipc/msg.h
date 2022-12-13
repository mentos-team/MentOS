/// @file msg.h
/// @brief Definition of structure for managing message queues.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
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
typedef struct msgbuf {
    /// Type of the message.
    long mtype;
    /// Text of the message.
    char mtext[1];
} msgbuf_t;

/// @brief Message queue data structure.
typedef struct msqid_ds {
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
} msqid_ds_t;

#ifdef __KERNEL__

/// @brief Get a System V message queue identifier.
/// @param key can be used either to obtain the identifier of a previously
/// created message queue, or to create a new set.
/// @param msgflg controls the behaviour of the function.
/// @return the message queue identifier, -1 on failure, and errno is set to
/// indicate the error.
long sys_msgget(key_t key, int msgflg);

/// @brief Used to send messages.
/// @param msqid the message queue identifier.
/// @param msgp points to a used-defined msgbuf.
/// @param msgsz specifies the size in bytes of mtext.
/// @param msgflg specifies the action to be taken in case of specific events.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long sys_msgsnd(int msqid, msgbuf_t *msgp, size_t msgsz, int msgflg);

/// @brief Used to receive messages.
/// @param msqid the message queue identifier.
/// @param msgp points to a used-defined msgbuf.
/// @param msgsz specifies the size in bytes of mtext.
/// @param msgtyp specifies the type of message requested, as follows:
/// - msgtyp == 0: the first message on the queue is received.
/// - msgtyp  > 0: the first message of type, msgtyp, is received.
/// - msgtyp  < 0: the first message of the lowest type that is less than or
///                equal to the absolute value of msgtyp is received.
/// @param msgflg specifies the action to be taken in case of specific events.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long sys_msgrcv(int msqid, msgbuf_t *msgp, size_t msgsz, long msgtyp, int msgflg);

/// @brief Message queue control operations.
/// @param msqid the message queue identifier.
/// @param cmd The command to perform.
/// @param buf used with IPC_STAT and IPC_SET.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long sys_msgctl(int msqid, int cmd, msqid_ds_t *buf);

#else

/// @brief Get a System V message queue identifier.
/// @param key can be used either to obtain the identifier of a previously
/// created message queue, or to create a new set.
/// @param msgflg controls the behaviour of the function.
/// @return the message queue identifier, -1 on failure, and errno is set to
/// indicate the error.
long msgget(key_t key, int msgflg);

/// @brief Used to send messages.
/// @param msqid the message queue identifier.
/// @param msgp points to a used-defined msgbuf.
/// @param msgsz specifies the size in bytes of mtext.
/// @param msgflg specifies the action to be taken in case of specific events.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long msgsnd(int msqid, msgbuf_t *msgp, size_t msgsz, int msgflg);

/// @brief Used to receive messages.
/// @param msqid the message queue identifier.
/// @param msgp points to a used-defined msgbuf.
/// @param msgsz specifies the size in bytes of mtext.
/// @param msgtyp specifies the type of message requested, as follows:
/// - msgtyp == 0: the first message on the queue is received.
/// - msgtyp  > 0: the first message of type, msgtyp, is received.
/// - msgtyp  < 0: the first message of the lowest type that is less than or
///                equal to the absolute value of msgtyp is received.
/// @param msgflg specifies the action to be taken in case of specific events.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long msgrcv(int msqid, msgbuf_t *msgp, size_t msgsz, long msgtyp, int msgflg);

/// @brief Message queue control operations.
/// @param msqid the message queue identifier.
/// @param cmd The command to perform.
/// @param buf used with IPC_STAT and IPC_SET.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long msgctl(int msqid, int cmd, msqid_ds_t *buf);

#endif
