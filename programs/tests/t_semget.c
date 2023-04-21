//test
#include "ipc/sem.h"
#include "stdio.h"
#include "ipc/ipc.h"
#include "sys/errno.h"
#include "stdlib.h"
#include "sys/unistd.h"

/*
Testing Semaphores.
This program creates a son and then performs a blocking operation on a semaphore. The son sleeps for 
five seconds and then it wakes up his father and then it deletes the semaphore.

*/

void semid_print(struct semid_ds *temp)
{
    printf("pid, IPC_KEY, Semid, semop, change: %d, %d, %d, %d, %d\n", temp->owner, temp->key, temp->semid, temp->sem_otime, temp->sem_ctime);
    for (int i = 0; i < (temp->sem_nsems); i++) {
        printf("%d semaphore:\n", i + 1);
        printf("value: %d, pid %d, process waiting %d\n", temp->sems[i].sem_val, temp->sems[i].sem_pid, temp->sems[i].sem_zcnt);
    }
}

int main()
{
    struct sembuf op;
    struct sembuf father[2];
    union semun arg;
    long ret, id;
    key_t key;

    // Checking if ftok works.
    key = ftok("/home/user/test7.txt", 5);
    printf("Key : %d\n", key);

    // Create the first semaphore.
    id = semget(2, 1, IPC_CREAT);
    printf("Id: %d\n", id);

    //long id2 = semget(IPC_PRIVATE, 1, IPC_CREAT);
    //long id2 = semget(2, 2, IPC_EXCL);  //need to return -1 bc already exists
    //long id3 = semget(2, 2, 0);  //need to return 3 (semid of the first)
    //long id4 = semget(100, 2, IPC_CREAT | IPC_EXCL);  //need to return 101 (new semaphore)

    // Set the value of the semaphore in the structure.
    arg.val = 0;
    // Setting the semaphore value.
    printf("Set Value (%d): %d\n", id, arg.val);
    ret = semctl(id, 0, SETVAL, &arg);
    if (ret == -1) {
        perror("Failed to set value of semaphore.");
        return 1;
    }

    // Check if we successfully set the value of the semaphore.
    ret = semctl(id, 0, GETVAL, &arg);
    printf("Check Value before(%d): %d\n", id, ret);

    // Create child process.
    if (!fork()) {
        // Make the child process sleep.
        sleep(2);

        // Initialize the operation structure.
        op.sem_num = 0; // Operate on semaphore 0.
        op.sem_op  = 1; // Increment value by 1.
        op.sem_flg = 0; // No flags.

        // This should allow the father process to decrement the semaphore by 2.

        // Perform the operation.
        semop(id, &op, 1);
        sleep(5);
        semop(id, &op, 1);
        // Remove the semaphore.
        //semctl(id, 0, IPC_RMID, NULL);

        // Exit with the child process.
        return 0;
    }

    // Initialize the operation structure.
    father[0].sem_num = 0;  // Operate on semaphore 0.
    father[0].sem_op  = -1; // Decrement value by 1.
    father[0].sem_flg = 0;  // No flags.
    father[1].sem_num = 0;  // Operate on semaphore 0.
    father[1].sem_op  = -1; // Decrement value by 1.
    father[1].sem_flg = 0;  // No flags.

    // We set the semaphore at the beginning at 1, and the child process will
    // increment the semaphore by 1, allowing us to decrement the semaphore by
    // 2.

    // Perform the operation.
    if (semop(id, father, 2) < 0) {
        printf("ERRORE\n");
    }
    
    // Check if we successfully set the value of the semaphore.
    ret = semctl(id, 0, GETVAL, &arg);
    printf("Check Value after(%d): %d\n", id, ret);

    /*if ((semid = semctl(id, 0, IPC_RMID, 0)) == -1){
        printf("%d\n", errno);
    }else{
        printf("found %d\n", semid);
    }*/

    //long id2 = semget(IPC_PRIVATE, 2, 0);
    //long id3 = semget(IPC_PRIVATE, 2, 0);
    //printf("id %d id2 %d id3 %d id4 %d\n", id, id2, id3, id4);
    /*if (i == -1){
        printf("Error with IPC_KEY\n");
        return -1;
    }*/

    return 0;
}