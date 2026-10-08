/// @file ipc.c
/// @brief Inter-Process Communication (IPC) system call implementation.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/ipc.h"

#include "errno.h"
#include "stdio.h"
#include "sys/msg.h"
#include "sys/sem.h"
#include "sys/shm.h"
#include "sys/stat.h"
#include "system/syscall_types.h"

// _syscall3(void *, shmat, int, shmid, const void *, shmaddr, int, shmflg)
void *shmat(int shmid, const void *shmaddr, int shmflg)
{
    long __res;
    __inline_syscall_3(__res, shmat, shmid, shmaddr, shmflg);
    __syscall_return(void *, __res);
}

// _syscall3(long, shmget, key_t, key, size_t, size, int, shmflg)
long shmget(key_t key, size_t size, int shmflg)
{
    long __res;
    __inline_syscall_3(__res, shmget, key, size, shmflg);
    __syscall_return(long, __res);
}

// _syscall1(long, shmdt, const void *, shmaddr)
long shmdt(const void *shmaddr)
{
    long __res;
    __inline_syscall_1(__res, shmdt, shmaddr);
    __syscall_return(long, __res);
}

// _syscall3(long, shmctl, int, shmid, int, cmd, struct shmid_ds *, buf)
long shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    long __res;
    __inline_syscall_3(__res, shmctl, shmid, cmd, buf);
    __syscall_return(long, __res);
}

// _syscall3(long, semget, key_t, key, int, nsems, int, semflg)
long semget(key_t key, int nsems, int semflg)
{
    long __res;
    __inline_syscall_3(__res, semget, key, nsems, semflg);
    __syscall_return(long, __res);
}

// _syscall4(long, semctl, int, semid, int, semnum, int, cmd, union semun *, arg)
long semctl(int semid, int semnum, int cmd, union semun *arg)
{
    long __res;
    __inline_syscall_4(__res, semctl, semid, semnum, cmd, arg);
    __syscall_return(long, __res);
}

// _syscall2(int, msgget, key_t, key, int, msgflg)
int msgget(key_t key, int msgflg)
{
    long __res;
    __inline_syscall_2(__res, msgget, key, msgflg);
    __syscall_return(int, __res);
}

// _syscall3(int, msgctl, int, msqid, int, cmd, struct msqid_ds *, buf)
int msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
    long __res;
    __inline_syscall_3(__res, msgctl, msqid, cmd, buf);
    __syscall_return(int, __res);
}

int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
    long __res;
    __inline_syscall_4(__res, msgsnd, msqid, msgp, msgsz, msgflg);
    __syscall_return(int, __res);
}

ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)
{
    long __res;
    __inline_syscall_5(__res, msgrcv, msqid, msgp, msgsz, msgtyp, msgflg);
    __syscall_return(int, __res);
}

long semop(int semid, struct sembuf *sops, unsigned nsops)
{
    long __res;

    // The pointer to the operation is NULL.
    if (!sops) {
        perror("The pointer to the operation is NULL.\n");
        errno = EINVAL;
        return -1;
    }

    // The value of nsops is negative.
    if (nsops <= 0) {
        perror("The value of nsops is negative.\n");
        errno = EINVAL;
        return -1;
    }

    // The kernel evaluates the complete array atomically.  Retrying one
    // operation at a time would change System V semantics and could deadlock
    // a multi-semaphore transaction.
    __inline_syscall_3(__res, semop, semid, sops, nsops);
    __syscall_return(long, __res);
}

key_t ftok(const char *path, int id)
{
    // Create a struct containing the serial number and the device number of the
    // file we use to generate the key.
    struct stat st;
    if (stat(path, &st) < 0) {
        printf("Cannot stat the file `%s`.\n", path);
        errno = ENOENT;
        return -1;
    }
    // Taking the upper 8 bits from the lower 8 bits of id, the second upper 8
    // bits from the lower 8 bits of the device number of the provided pathname,
    // and the lower 16 bits from the lower 16 bits of the inode number of the
    // provided pathname.
    return (st.st_ino & 0xffff) | ((st.st_dev & 0xff) << 16) | ((id & 0xffU) << 24);
}
