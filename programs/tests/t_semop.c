/// @file t_semop.c
/// @brief Demonstrates the use of semaphores between processes using semop.
/// Each process modifies a semaphore and outputs part of a message in order.
/// @copyright (c) 2014-2024
/// This file is distributed under the MIT License. See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    // Define semaphore operations for the different processes
    struct sembuf sops[6];

    // Process 1 (decrement semaphore 0)
    sops[0].sem_num = 0;
    sops[0].sem_op  = -1;
    sops[0].sem_flg = 0;

    // Process 2 (decrement semaphore 1)
    sops[1].sem_num = 1;
    sops[1].sem_op  = -1;
    sops[1].sem_flg = 0;

    // Process 3 (decrement semaphore 2)
    sops[2].sem_num = 2;
    sops[2].sem_op  = -1;
    sops[2].sem_flg = 0;

    // Process 2 (increment semaphore 0)
    sops[3].sem_num = 0;
    sops[3].sem_op  = 1;
    sops[3].sem_flg = 0;

    // Process 3 (increment semaphore 1)
    sops[4].sem_num = 1;
    sops[4].sem_op  = 1;
    sops[4].sem_flg = 0;

    // Process 4 (increment semaphore 2)
    sops[5].sem_num = 2;
    sops[5].sem_op  = 1;
    sops[5].sem_flg = 0;

    int status;

    // ========================================================================
    // Create a semaphore set with 4 semaphores.
    int semid = semget(17, 4, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (semid == -1) {
        perror("Failed to create semaphore set");
        return 1;
    }

    // ========================================================================
    // Set initial semaphore values: {0, 0, 0, 1}
    unsigned short values[] = {0, 0, 0, 1}; // Last semaphore is initialized to 1 for control.
    union semun arg;
    arg.array = values;

    if (semctl(semid, 0, SETALL, &arg) == -1) {
        perror("Failed to set semaphore values");
        return 1;
    }

    // ========================================================================
    // Create child processes and perform semaphore operations.

    // Process 1: Wait for semaphore 0 to decrement, then print "cheers!"
    if (!fork()) {
        semop(semid, &sops[0], 1);
        printf("cheers!\n");
        exit(0);
    }

    // Process 2: Wait for semaphore 1, print "course, ", then increment semaphore 0
    if (!fork()) {
        semop(semid, &sops[1], 1);
        printf("course, ");
        semop(semid, &sops[3], 1); // Increment semaphore 0 to signal Process 1.
        exit(0);
    }

    // Process 3: Wait for semaphore 2, print "systems ", then increment semaphore 1
    if (!fork()) {
        semop(semid, &sops[2], 1);
        printf("systems ");
        semop(semid, &sops[4], 1); // Increment semaphore 1 to signal Process 2.
        exit(0);
    }

    // Process 4: Print "From the operating ", then increment semaphore 2 to start Process 3
    if (!fork()) {
        printf("From the operating ");
        semop(semid, &sops[5], 1); // Increment semaphore 2 to signal Process 3.
        exit(0);
    }

    // ========================================================================
    // Wait for all child processes to finish.
    for (int n = 0; n < 4; n++) {
        wait(&status);
    }

    // ========================================================================
    // Remove the semaphore set after all processes are done.
    if (semctl(semid, 0, IPC_RMID, 0) == -1) {
        perror("Failed to remove semaphore set");
        return 1;
    }

    return 0;
}
