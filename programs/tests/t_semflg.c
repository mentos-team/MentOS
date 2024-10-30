/// @file t_semflg.c
/// @brief Tests the use of semaphores and some IPC flags.
/// @details This program creates a semaphore set, performs operations on the
/// semaphore from both parent and child processes, and demonstrates basic IPC
/// mechanisms using semaphores. The parent and child process modify and check
/// the semaphore value. The semaphore set is deleted once the operations are
/// complete.
/// @copyright (c) 2014-2024
/// This file is distributed under the MIT License. See LICENSE.md for details.

#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <strerror.h>

int main(int argc, char *argv[])
{
    struct sembuf op[1]; // Operation structure for semaphore operations.
    union semun arg;     // Union to store semaphore values.
    long ret, semid;     // Return values and semaphore ID.

    // ========================================================================
    // Create a semaphore set with one semaphore.
    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (semid < 0) {
        perror("Failed to create semaphore set");
        return EXIT_FAILURE;
    }
    printf("[parent] Created semaphore set (semid: %ld)\n", semid);

    // ========================================================================
    // Set the value of the semaphore to 1.
    arg.val = 1;
    ret     = semctl(semid, 0, SETVAL, &arg);
    if (ret < 0) {
        perror("Failed to set value of semaphore");
        return EXIT_FAILURE;
    }
    printf("[parent] Set semaphore value to 1 (semid: %ld)\n", semid);

    // ========================================================================
    // Get and verify the semaphore value.
    ret = semctl(semid, 0, GETVAL, NULL);
    if (ret < 0) {
        perror("Failed to get value of semaphore");
        return EXIT_FAILURE;
    }
    printf("[parent] Semaphore value is %ld (expected: 1)\n", ret);

    // ========================================================================
    // Fork a child process to manipulate the semaphore.
    if (fork() == 0) {
        struct sembuf op_child;
        // Initialize the operation to increment the semaphore by 1.
        op_child.sem_num = 0; // Operate on semaphore 0.
        op_child.sem_op  = 1; // Increment by 1.
        op_child.sem_flg = 0; // No special flags.

        sleep(3); // Simulate delay before child performs the operation.

        // Perform the increment operation on the semaphore.
        if (semop(semid, &op_child, 1) < 0) {
            perror("[child] Failed to perform semaphore operation");
            return EXIT_FAILURE;
        }
        printf("[child] Successfully incremented semaphore (semid: %ld)\n", semid);

        // Check the updated value of the semaphore.
        ret = semctl(semid, 0, GETVAL, NULL);
        if (ret < 0) {
            perror("[child] Failed to get value of semaphore");
            return EXIT_FAILURE;
        }
        printf("[child] Semaphore value is %ld (expected: 2)\n", ret);
        printf("[child] Exiting now.\n");
        return EXIT_SUCCESS;
    }

    // Parent process prepares to perform a decrement operation on the semaphore.
    op[0].sem_num = 0;  // Operate on semaphore 0.
    op[0].sem_op  = -2; // Decrement by 2 (this will block if the semaphore value is less than 2).
    op[0].sem_flg = 0;  // No special flags.

    // ========================================================================
    // Perform the decrement operation on the semaphore.
    if (semop(semid, op, 1) < 0) {
        perror("[parent] Failed to perform semaphore operation");
        return EXIT_FAILURE;
    }
    printf("[parent] Successfully performed semaphore operations (semid: %ld)\n", semid);

    // ========================================================================
    // Get and verify the final value of the semaphore.
    ret = semctl(semid, 0, GETVAL, NULL);
    if (ret < 0) {
        perror("[parent] Failed to get value of semaphore");
        return EXIT_FAILURE;
    }
    printf("[parent] Semaphore value is %ld (expected: 0)\n", ret);

    // ========================================================================
    // Remove the semaphore set.
    ret = semctl(semid, 0, IPC_RMID, 0);
    if (ret < 0) {
        perror("[parent] Failed to remove semaphore set");
        return EXIT_FAILURE;
    }
    printf("[parent] Successfully removed semaphore set (semid: %ld)\n", semid);

    return EXIT_SUCCESS;
}
