/// @file ipc.c
/// @brief Inter-Process Communication (IPC) system call implementation.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "system/syscall_types.h"
#include "sys/errno.h"
#include "stddef.h"
#include "string.h"
#include "io/debug.h"
#include "stdio.h"
#include "ipc/sem.h"
#include "ipc/shm.h"
#include "ipc/msg.h"
#include "stdlib.h"
#include "sys/stat.h"

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




long ipcs(int argc, char** argv){

    long __res;
    __inline_syscall0(__res, semipcs);
    __syscall_return(long, __res);

    /*if (argc>4)return -1;
    if (argc == 1){ //default
        long __res;
        __inline_syscall0(__res, semipcs);
        return __res;
    }
    if (argc == 2){
        if (!strcmp(argv[1], "-s")){ //semaphores
            long __res;
            __inline_syscall0(__res, semipcs);
            return __res;
        }

        if (!strcmp(argv[1], "-m")) { //shared memories
            printf("Not Implemented!\n");
            return 0;
        }

        if (!strcmp(argv[1], "-q")){ //message queues
            printf("Not Implemented!\n");
            return 0;
        }

        return -1;        
    }
    else{
        if(!strcmp(argv[1], "-i" ) && !strcmp(argv[3],"-s")){
            union semun temp;
            temp.buf = (struct semid_ds *)malloc(sizeof(struct semid_ds));
            long __res;
            __inline_syscall4(__res, semctl, atoi(argv[2]), 0, GETNSEMS, NULL);
            temp.buf -> sems = (struct sem *)malloc(sizeof(struct sem) * __res);
            
            __inline_syscall4(__res, semctl, atoi(argv[2]), 0, IPC_STAT, &temp);
            printf("%d\t\t%d\t\t%d\t\t\t%d\n", temp.buf->key, temp.buf->semid, temp.buf->owner, temp.buf->sem_nsems);
            if(__res == -1)
                return -1;
            return __res;
        }
        if(!strcmp(argv[1], "-i" ) && !strcmp(argv[3],"-m")){
            printf("Not Implemented!\n");
            return 0;
        }
        if(!strcmp(argv[1], "-i" ) && !strcmp(argv[3],"-q")){
            printf("Not Implemented!\n");
            return 0;
        }
        return -1;
    }


    return 0;*/
    
}

long semop(int semid, struct sembuf *sops, unsigned nsops)
{

    long __res;
    // Check the arguments.
    if ((nsops <= 0) || (sops == NULL)) {
        errno = EINVAL;
        return -1;
    }
    // The process continues to try to perform the operation until it completes
    // or receives an error.
    while (1) {
        // Calling the kernel-side function.
        __inline_syscall3(__res, semop, semid, sops, nsops);
        // If we get an error we stop the loop.
        if (__res != OPERATION_NOT_ALLOWED)
            break;
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
        //printf("Error finding the serial number, check Errno...\n");
        return -1;
    }
    // Taking the upper 8 bits from the lower 8 bits of id, the second upper 8
    // bits from the lower 8 bits of the device number of the provided pathname,
    // and the lower 16 bits from the lower 16 bits of the inode number of the
    // provided pathname.
    return ((st.st_ino & 0xffff) | ((st.st_dev & 0xff) << 16) | ((id & 0xffu) << 24));
}
