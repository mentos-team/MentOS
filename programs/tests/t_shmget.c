
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
    int shmid;
    pid_t cpid;
    int *array;

    // Create shared memory.
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
    while (wait(NULL) != -1) continue;
    
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
