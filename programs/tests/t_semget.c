/// @file t_semget.c
/// @brief This program creates a son and then performs a blocking operation on
/// a semaphore. The son sleeps for  five seconds and then it wakes up his
/// father and then it deletes the semaphore.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    struct sembuf op_child;
    struct sembuf op_father[3];
    union semun arg;
    long ret, semid;
    key_t key;

    // ========================================================================
    // Generating a key using ftok
    key = ftok("/README", 5);
    if (key < 0) {
        perror("Failed to generate key using ftok.");
        return 1;
    }
    printf("Generated key using ftok (key = %d)\n", key);

    // ========================================================================
    // Create the first semaphore.
    semid = semget(key, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (semid < 0) {
        perror("Failed to create semaphore set.");
        return 1;
    }
    printf("[father] Created semaphore set (id : %d)\n", semid);

    // ========================================================================
    // Set the value of the semaphore in the structure.
    arg.val = 1;
    // Setting the semaphore value.
    ret = semctl(semid, 0, SETVAL, &arg);
    if (ret < 0) {
        perror("Failed to set value of semaphore.");
        return 1;
    }
    printf("[father] Set semaphore value (id : %d, value : %d == 1)\n", semid, arg.val);

    // ========================================================================
    // Check if we successfully set the value of the semaphore.
    ret = semctl(semid, 0, GETVAL, NULL);
    if (ret < 0) {
        perror("Failed to get the value of semaphore set.");
        return 1;
    }
    printf("[father] Get semaphore value (id : %d, value : %d == 1)\n", semid, ret);

    // ========================================================================
    // Create child process.
    if (!fork()) {
        // Initialize the operation structure.
        op_child.sem_num = 0; // Operate on semaphore 0.
        op_child.sem_op  = 1; // Increment value by 1.
        op_child.sem_flg = 0; // No flags.

        // Semaphore has value 1.
        sleep(3);
        if (semop(semid, &op_child, 1) < 0) {
            perror("Failed to perform first child operation.");
            return 1;
        }
        printf("[child] Succesfully performed opeation (id : %d)\n", semid);

        // Check if we successfully set the value of the semaphore.
        ret = semctl(semid, 0, GETVAL, NULL);
        if (ret < 0) {
            perror("Failed to get the value of semaphore set.");
            return 1;
        }
        printf("[child] Get semaphore value (id : %d, value : %d == 0)\n", semid, ret);

        // Semaphore has value 2, now the father can continue.
        sleep(3);
        if (semop(semid, &op_child, 1) < 0) {
            perror("Failed to perform second child operation.");
            return 1;
        }
        printf("[child] Succesfully performed opeation (id : %d)\n", semid);

        // Check if we successfully set the value of the semaphore.
        ret = semctl(semid, 0, GETVAL, NULL);
        if (ret < 0) {
            perror("Failed to get the value of semaphore set.");
            return 1;
        }
        printf("[child] Get semaphore value (id : %d, value : %d == 0)\n", semid, ret);

        printf("[child] Exit, now.\n", semid, ret);

        // Exit with the child process.
        return 0;
    }

    // ========================================================================
    // Initialize the operation structure.
    op_father[0].sem_num = 0;  // Operate on semaphore 0.
    op_father[0].sem_op  = -1; // Decrement value by 1.
    op_father[0].sem_flg = 0;  // No flags.
    op_father[1].sem_num = 0;  // Operate on semaphore 0.
    op_father[1].sem_op  = -1; // Decrement value by 1.
    op_father[1].sem_flg = 0;  // No flags.
    op_father[2].sem_num = 0;  // Operate on semaphore 0.
    op_father[2].sem_op  = -1; // Decrement value by 1.
    op_father[2].sem_flg = 0;  // No flags.

    // ========================================================================
    // Perform the operations.
    if (semop(semid, op_father, 3) < 0) {
        perror("Failed to perform father operation.");
        return 1;
    }

    sleep(2);

    printf("[father] Performed semaphore operations (id : %d)\n", semid);

    // Check if we successfully set the value of the semaphore.
    ret = semctl(semid, 0, GETVAL, NULL);
    if (ret < 0) {
        perror("Failed to get the value of semaphore set.");
        return 1;
    }
    printf("[father] Get semaphore value (id : %d, value : %d == 0)\n", semid, ret);

    // Delete the semaphore set.
    ret = semctl(semid, 0, IPC_RMID, 0);
    if (ret < 0) {
        perror("Failed to remove semaphore set.");
    }
    printf("[father] Correctly removed semaphore set.\n");

    return 0;
}
