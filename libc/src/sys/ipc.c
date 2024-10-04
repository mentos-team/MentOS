/// @file ipc.c
/// @brief Inter-Process Communication (IPC) system call implementation.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/ipc.h"

#include "io/debug.h"
#include "io/debug.h"
#include "stddef.h"
#include "stdio.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/errno.h"
#include "sys/msg.h"
#include "sys/sem.h"
#include "sys/shm.h"
#include "sys/stat.h"
#include "sys/unistd.h"
#include "system/syscall_types.h"

_syscall3(void *, shmat, int, shmid, const void *, shmaddr, int, shmflg)

_syscall3(long, shmget, key_t, key, size_t, size, int, flag)

_syscall1(long, shmdt, const void *, shmaddr)

_syscall3(long, shmctl, int, shmid, int, cmd, struct shmid_ds *, buf)

_syscall3(long, semget, key_t, key, int, nsems, int, semflg)

_syscall4(long, semctl, int, semid, int, semnum, int, cmd, union semun *, arg)

_syscall2(int, msgget, key_t, key, int, msgflg)

_syscall3(int, msgctl, int, msqid, int, cmd, struct msqid_ds *, buf)

int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
    long __res;
    do {
        __inline_syscall4(__res, msgsnd, msqid, msgp, msgsz, msgflg);
        if (!(msgflg & IPC_NOWAIT) && (__res == -EAGAIN)) {
            continue;
        }
        break;
    } while (1);
    __syscall_return(int, __res);
}

ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)
{
    long __res;
    do {
        __inline_syscall5(__res, msgrcv, msqid, msgp, msgsz, msgtyp, msgflg);
        if (!(msgflg & IPC_NOWAIT) && ((__res == -EAGAIN) || (__res == -ENOMSG))) {
            continue;
        }
        break;
    } while (1);
    __syscall_return(int, __res);
}

long semop(int semid, struct sembuf *sops, unsigned nsops)
{
    struct sembuf *op;
    long __res;

    // The pointer to the operation is NULL.
    if (!sops) {
        pr_err("The pointer to the operation is NULL.\n");
        errno = EINVAL;
        return -1;
    }

    // The value of nsops is negative.
    if (nsops <= 0) {
        pr_err("The value of nsops is negative.\n");
        errno = EINVAL;
        return -1;
    }

    // This should be performed for each sops.
    for (size_t i = 0; i < nsops; i++) {
        // Get the operation.
        op = &sops[i];
        // The process continues to try to perform the operation until it completes
        // or receives an error.
        while (1) {
            // Calling the kernel-side function.
            __inline_syscall3(__res, semop, semid, op, 1);

            // If we get an error, the operation has been taken care of we stop
            // the loop. We also stop the loop if the operation is not allowed
            // and the IPC_NOWAIT flag is 1
            if ((__res != -EAGAIN) || (op->sem_flg & IPC_NOWAIT)) {
                break;
            }
        }
        // If the operation couldn't be performed and we had the IPC_NOWAIT set
        // to 1 then we return.
        if ((__res == -EAGAIN) && (op->sem_flg & IPC_NOWAIT)) {
            errno = EAGAIN;
            return -1;
        }
    }

    // Now, we can return the value.
    __syscall_return(long, __res);
}

key_t ftok(char *path, int id)
{
    // Create a struct containing the serial number and the device number of the
    // file we use to generate the key.
    struct stat st;
    if (stat(path, &st) < 0) {
        pr_err("Cannot stat the file `%s`...\n", path);
        errno = ENOENT;
        return -1;
    }
    // Taking the upper 8 bits from the lower 8 bits of id, the second upper 8
    // bits from the lower 8 bits of the device number of the provided pathname,
    // and the lower 16 bits from the lower 16 bits of the inode number of the
    // provided pathname.
    return (st.st_ino & 0xffff) | ((st.st_dev & 0xff) << 16) | ((id & 0xffu) << 24);
}
