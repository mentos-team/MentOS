/// @file ipc.c
/// @brief Inter-Process Communication (IPC) system call implementation.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "system/syscall_types.h"
#include "sys/errno.h"
#include "stddef.h"
#include "ipc/sem.h"
#include "ipc/shm.h"
#include "ipc/msg.h"
//#include "fs/vfs_types.h"
//#include "fs/vfs.h"
#include "fcntl.h"
#include "sys/stat.h"
#include "stdio.h"

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

key_t ftok(char *path, int id){


    /*int fd;

    fd=open(path, O_RDONLY, 0777);*/
    
    /*
        if (() == -1){
        errno = ENOENT;
        return -1;
    }
    */
    struct stat_t st;   //struct containing the serial number and the device number of the file 
    if (stat(path, &st)<0){
        errno = ENOENT;
        //printf("Error finding the serial number, check Errno...\n");
        return -1;   
    }
    //printf("Serial number: %d\n", st.st_ino);


    //taking the upper 8 bits from the lower 8 bits of id, the second upper 8 bits from the lower 8 bits of the device number of the provided pathname, and the lower 16 bits from the lower 16 bits of the inode number of the provided pathname

    return ((st.st_ino & 0xffff) | ((st.st_dev & 0xff) << 16) | ((id & 0xffu) << 24));

}

