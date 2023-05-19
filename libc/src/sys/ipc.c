/// @file ipc.c
/// @brief Inter-Process Communication (IPC) system call implementation.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "sys/errno.h"
#include "sys/stat.h"
#include "sys/sem.h"
#include "sys/shm.h"
#include "sys/msg.h"

#include "system/syscall_types.h"
#include "stddef.h"
#include "string.h"
#include "io/debug.h"
#include "stdio.h"
#include "stdlib.h"
#include "io/debug.h"
#include "stdio.h"

_syscall3(void *, shmat, int, shmid, const void *, shmaddr, int, shmflg)

_syscall3(long, shmget, key_t, key, size_t, size, int, flag)

_syscall1(long, shmdt, const void *, shmaddr)

_syscall3(long, shmctl, int, shmid, int, cmd, struct shmid_ds *, buf)

_syscall3(long, semget, key_t, key, int, nsems, int, semflg)

_syscall4(long, semctl, int, semid, int, semnum, int, cmd, union semun *, arg)

_syscall2(long, msgget, key_t, key, int, msgflg)

_syscall4(long, msgsnd, int, msqid, struct msgbuf *, msgp, size_t, msgsz, int, msgflg)

_syscall5(long, msgrcv, int, msqid, struct msgbuf *, msgp, size_t, msgsz, long, msgtyp, int, msgflg)

_syscall3(long, msgctl, int, msqid, int, cmd, struct msqid_ds *, buf)

long semop(int semid, struct sembuf *sops, unsigned nsops)
{
    struct sembuf *op;
    long __res;

    // The pointer to the operation is NULL.
    if (!sops) {
        pr_err("The pointer to the operation is NULL.\n");
        return -EINVAL;
    }

    // The value of nsops is negative.
    if (nsops <= 0) {
        pr_err("The value of nsops is negative.\n");
        return -EINVAL;
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
            if ((__res != -EAGAIN) || (op->sem_flg & IPC_NOWAIT))
                break;
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
    struct stat_t st;
    if (stat(path, &st) < 0) {
        errno = ENOENT;
        pr_debug("Error finding the serial number, check Errno...\n");
        return -1;
    }
    // Taking the upper 8 bits from the lower 8 bits of id, the second upper 8
    // bits from the lower 8 bits of the device number of the provided pathname,
    // and the lower 16 bits from the lower 16 bits of the inode number of the
    // provided pathname.
    return ((st.st_ino & 0xffff) | ((st.st_dev & 0xff) << 16) | ((id & 0xffu) << 24));
}
