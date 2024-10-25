/// @file t_semget.c
/// @brief This program demonstrates semaphore operations between a parent and
/// child process. The child process increments a semaphore after sleeping,
/// which unblocks the parent process. Finally, the semaphore is deleted by the
/// child process before exiting.
/// @copyright (c) 2014-2024
/// This file is distributed under the MIT License. See LICENSE.md for details.

#include <sys/unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <strerror.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    struct sembuf op_child;     // Semaphore operation for the child.
    struct sembuf op_father[3]; // Semaphore operations for the father.
    union semun arg;            // Union for semctl operations.
    long ret, semid;            // Return values and semaphore ID.
    key_t key;                  // Key for semaphore.

    // ========================================================================
    // Generate a unique key using ftok.
    key = ftok("/README.md", 5);
    if (key < 0) {
        perror("Failed to generate key using ftok");
        return 1;
    }
    printf("Generated key using ftok (key = %d)\n", key);

    // ========================================================================
    // Create a semaphore set with one semaphore.
    semid = semget(key, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (semid < 0) {
        perror("Failed to create semaphore set");
        return 1;
    }
    printf("[father] Created semaphore set (id : %d)\n", semid);

    // ========================================================================
    // Set the value of the semaphore to 1.
    arg.val = 1;
    ret     = semctl(semid, 0, SETVAL, &arg);
    if (ret < 0) {
        perror("Failed to set semaphore value");
        return 1;
    }
    printf("[father] Set semaphore value to 1 (id : %d)\n", semid);

    // ========================================================================
    // Verify that the semaphore value is set correctly.
    ret = semctl(semid, 0, GETVAL, NULL);
    if (ret < 0) {
        perror("Failed to get semaphore value");
        return 1;
    }
    printf("[father] Semaphore value is %d (expected: 1)\n", ret);

    // ========================================================================
    // Fork a child process.
    if (fork() == 0) {
        // Child process setup.
        op_child.sem_num = 0; // Operate on semaphore 0.
        op_child.sem_op  = 1; // Increment semaphore by 1.
        op_child.sem_flg = 0; // No special flags.

        sleep(3); // Simulate some work before modifying the semaphore.

        // Increment the semaphore, unblocking the parent.
        if (semop(semid, &op_child, 1) < 0) {
            perror("Failed to perform child semaphore operation");
            return 1;
        }
        printf("[child] Performed first semaphore operation (id: %d)\n", semid);

        // Verify the updated semaphore value.
        ret = semctl(semid, 0, GETVAL, NULL);
        if (ret < 0) {
            perror("Failed to get semaphore value in child");
            return 1;
        }
        printf("[child] Semaphore value is %d (expected: 2)\n", ret);

        // Sleep and perform another increment operation.
        sleep(3);
        if (semop(semid, &op_child, 1) < 0) {
            perror("Failed to perform second child semaphore operation");
            return 1;
        }
        printf("[child] Performed second semaphore operation (id: %d)\n", semid);

        // Check final semaphore value.
        ret = semctl(semid, 0, GETVAL, NULL);
        if (ret < 0) {
            perror("Failed to get final semaphore value in child");
            return 1;
        }
        printf("[child] Final semaphore value is %d\n", ret);

        // Delete the semaphore set.
        ret = semctl(semid, 0, IPC_RMID, 0);
        if (ret < 0) {
            perror("Failed to remove semaphore set in child");
            return 1;
        }
        printf("[child] Removed semaphore set (id: %d)\n", semid);

        // Exit the child process.
        return 0;
    }

    // ========================================================================
    // Parent process: Prepare operations to decrement semaphore.
    op_father[0].sem_num = 0;  // Operate on semaphore 0.
    op_father[0].sem_op  = -1; // Decrement by 1.
    op_father[0].sem_flg = 0;  // No special flags.

    op_father[1].sem_num = 0;  // Operate on semaphore 0.
    op_father[1].sem_op  = -1; // Decrement by 1.
    op_father[1].sem_flg = 0;  // No special flags.

    op_father[2].sem_num = 0;  // Operate on semaphore 0.
    op_father[2].sem_op  = -1; // Decrement by 1.
    op_father[2].sem_flg = 0;  // No special flags.

    // ========================================================================
    // Perform the blocking semaphore operations.
    if (semop(semid, op_father, 3) < 0) {
        perror("Failed to perform parent semaphore operations");
        return 1;
    }
    printf("[father] Performed semaphore operations (id: %d)\n", semid);

    // Verify that the semaphore value is updated correctly.
    ret = semctl(semid, 0, GETVAL, NULL);
    if (ret < 0) {
        perror("Failed to get semaphore value in parent");
        return 1;
    }
    printf("[father] Semaphore value is %d (expected: 0)\n", ret);

    // Wait for the child process to terminate.
    if (wait(NULL) == -1) {
        fprintf(stderr, "Failed to wait for child process: %s\n", strerror(errno));
        return EXIT_FAILURE; // Return failure if wait fails.
    }

    return 0;
}
