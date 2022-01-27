/// @file ipc.c
/// @brief Inter-Process Communication (IPC) system call implementation.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "system/syscall_types.h"
#include "sys/errno.h"
#include "stddef.h"
#include "ipc/sem.h"
#include "ipc/shm.h"
#include "ipc/msg.h"

_syscall3(void *, shmat, int, shmid, const void *, shmaddr, int, shmflg)

_syscall3(long, shmget, key_t, key, size_t, size, int, flag)

_syscall1(long, shmdt, const void *, shmaddr)

_syscall3(long, shmctl, int, shmid, int, cmd, struct shmid_ds *, buf)

_syscall3(long, semget, key_t, key, int, nsems, int, semflg)

_syscall3(long, semop, int, semid, struct sembuf *, sops, unsigned, nsops)

_syscall4(long, semctl, int, semid, int, semnum, int, cmd, unsigned long, arg)

_syscall2(long, msgget, key_t, key, int, msgflg)

_syscall4(long, msgsnd, int, msqid, struct msgbuf *, msgp, size_t, msgsz, int, msgflg)

_syscall5(long, msgrcv, int, msqid, struct msgbuf *, msgp, size_t, msgsz, long, msgtyp, int, msgflg)

_syscall3(long, msgctl, int, msqid, int, cmd, struct msqid_ds *, buf)

