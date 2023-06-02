/// @file t_semflg.c
/// @brief Tests some of the IPC flags.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/unistd.h"
#include "sys/errno.h"
#include "sys/sem.h"
#include "sys/ipc.h"
#include "stdlib.h"
#include "fcntl.h"
#include "stdio.h"

int main(int argc, char *argv[])
{
    struct sembuf op[1];
    union semun arg;
    long ret, semid;

    op[0].sem_num = 0;
    op[0].sem_op  = -2;
    op[0].sem_flg = IPC_NOWAIT;

    // ========================================================================
    // Create the first semaphore.
    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (semid < 0) {
        perror("Failed to create semaphore set");
        return 1;
    }
    printf("[father] Created semaphore set (semid : %d)\n", semid);

    // ========================================================================
    // Set the value of the semaphore in the structure.
    arg.val = 1;
    // Setting the semaphore value.
    ret = semctl(semid, 0, SETVAL, &arg);
    if (ret < 0) {
        perror("Failed to set value of semaphore");
        return 1;
    }
    printf("[father] Set semaphore value (id : %d, value : %d == 1)\n", semid, arg.val);

    // ========================================================================
    // Check if we successfully set the value of the semaphore.
    ret = semctl(semid, 0, GETVAL, NULL);
    if (ret < 0) {
        perror("Failed to get the value of semaphore set");
        return 1;
    }
    printf("[father] Get semaphore value (id : %d, value : %d == 1)\n", semid, ret);

    // ========================================================================
    // Create child process.
    if (!fork()) {
        struct sembuf op_child;
        // Initialize the operation structure.
        op_child.sem_num = 0; // Operate on semaphore 0.
        op_child.sem_op  = 1; // Increment value by 1.
        op_child.sem_flg = 0; // No flags.

        sleep(3);

        // ====================================================================
        // Increment semaphore value.
        if (semop(semid, &op_child, 1) < 0) {
            perror("Failed to perform first child operation");
            return 1;
        }
        printf("[child] Succesfully performed opeation (id : %d)\n", semid);

        // ====================================================================
        // Check if we successfully set the value of the semaphore.
        ret = semctl(semid, 0, GETVAL, NULL);
        if (ret < 0) {
            perror("Failed to get the value of semaphore set");
            return 1;
        }
        printf("[child] Get semaphore value (id : %d, value : %d == 1)\n", semid, ret);
        printf("[child] Exit, now.\n", semid, ret);

        // ========================================================================
        // Delete the semaphore set.
        ret = semctl(semid, 0, IPC_RMID, 0);
        if (ret < 0) {
            perror("Failed to remove semaphore set");
        }
        printf("[child] Correctly removed semaphore set.\n");

        return 0;
    }

    // ========================================================================
    // Perform the operations.
    if (semop(semid, op, 2) < 0) {
        perror("Failed to perform operation");
        return 1;
    }
    printf("[father] Performed semaphore operations (id : %d)\n", semid);

    // ========================================================================
    // Check if we successfully set the value of the semaphore.
    ret = semctl(semid, 0, GETVAL, NULL);
    if (ret < 0) {
        perror("Failed to get the value of semaphore set");
        return 1;
    }
    printf("[father] Get semaphore value (id : %d, value : %d == 1)\n", semid, ret);

    // ========================================================================
    // Delete the semaphore set.
    ret = semctl(semid, 0, IPC_RMID, 0);
    if (ret < 0) {
        perror("Failed to remove semaphore set");
    }
    printf("[father] Correctly removed semaphore set.\n");

    return 0;
}
