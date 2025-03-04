/// @file t_shmget.c
/// @brief Demonstrates the creation and usage of shared memory between a parent
/// and child process.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

/// Define the size of shared memory to hold two integers.
#define MEM_SIZE (sizeof(int) * 2)

int main(void)
{
    int shmid;
    pid_t cpid;
    int *array;

    // Create shared memory segment with IPC_PRIVATE key and specified memory size.
    shmid = shmget(IPC_PRIVATE, MEM_SIZE, IPC_CREAT | 0600);
    if (shmid == -1) {
        perror("shmget");
        return EXIT_FAILURE;
    }
    printf("shmid = %d;\n", shmid);

    // Create a child.
    cpid = fork();
    if (cpid == 0) {
        // Child attaches the shared memory.
        array = (int *)shmat(shmid, NULL, 0);
        if (array == NULL) {
            perror("shmat");
            return EXIT_FAILURE;
        }
        printf("C: %p\n", array);
        array[0] = 1;
        return 0;
    }

    // Father attaches the shared memory.
    array = (int *)shmat(shmid, NULL, 0);
    if (array == NULL) {
        perror("shmat");
        return EXIT_FAILURE;
    }

    // Wait for the child to finish.
    while (wait(NULL) != -1) {
    }

    printf("F: %p\n", array);
    array[1] = 2;

    printf("array[%d] : %d\n", 0, array[0]);
    printf("array[%d] : %d\n", 1, array[1]);

    // Detatch the shared memory.
    if (shmdt(array) < 0) {
        perror("shmdt");
        return EXIT_FAILURE;
    }

    // Remove the shared memory.
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
