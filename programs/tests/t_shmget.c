
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/unistd.h>
#include <sys/wait.h>

#define MEM_SIZE sizeof(int) * 2

int main(void)
{
    int semid, shmid;
    pid_t cpid;
    int *array;

    // Create shared memory.
    shmid = shmget(IPC_PRIVATE, MEM_SIZE, IPC_CREAT | 0600);
    if (shmid == -1) {
        perror("shmget");
        return EXIT_FAILURE;
    }
    // Create a semaphore set containing one semaphore.
    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (semid == -1) {
        perror("semget");
        return EXIT_FAILURE;
    }

    cpid = fork();

    if (cpid == 0) {
        array = (int *)shmat(shmid, NULL, 0);
        printf("C: %p\n", array);
        array[0] = 1;
        return 0;
    } else {
        array = (int *)shmat(shmid, NULL, 0);
        printf("F: %p\n", array);
        array[1] = 2;
    }

    while (wait(NULL) != -1) continue;

    printf("array[%d] : %d\n", 0, array[0]);
    printf("array[%d] : %d\n", 1, array[1]);

    // Remove the shared memory.
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        return EXIT_FAILURE;
    }
    // Remove the semaphore set.
    if (semctl(semid, 0, IPC_RMID, NULL) == -1) {
        perror("semctl");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
