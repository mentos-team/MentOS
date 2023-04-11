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


void semid_print(struct semid_ds *temp){
    printf("pid, IPC_KEY, Semid, semop, change: %d, %d, %d, %d, %d\n", temp->owner, temp->key, temp->semid, temp->sem_otime, temp->sem_ctime);
    for(int i=0; i<(temp->sem_nsems); i++){
        printf("%d semaphore:\n", i+1);
        printf("value: %d, pid %d, process waiting %d\n", temp->sems[i].sem_val, temp->sems[i].sem_pid, temp->sems[i].sem_zcnt);
    }
}

int main()
{      
    union semun arg;
    arg.val = 1; 
    //key_t i = ftok("/test1.txt", 5);  //checking ftok
    long id = semget(2,1,IPC_CREAT);  //first semaphores
    //long id2 = semget(IPC_PRIVATE, 1, IPC_CREAT);
    //long id2 = semget(2, 2, IPC_EXCL);  //need to return -1 bc already exists

    //long id3 = semget(2, 2, 0);  //need to return 3 (semid of the first)

    //long id4 = semget(100, 2, IPC_CREAT | IPC_EXCL);  //need to return 101 (new semaphore)
    long value;
    printf("ID: %d\n", id);
    if ((value = semctl(id, 0, SETVAL, &arg)) == -1){
        printf("ERRORE\n");
    }else{
        //semid_print(arg.v);
        printf("valore sem 0: %d  \n", semctl(id, 0, GETVAL, &arg));
    }



    if (!fork()){
        sleep(5);
        struct sembuf op2;
        op2.sem_num = 0;
        op2.sem_op = 150;
        op2.sem_flg = 0;
        semop(id, &op2, 1);

        semctl(id, 0, IPC_RMID, NULL);

        exit(0);
    }

    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = 0;
    if (semop(id, &op, 2) < 0){
        printf("ERRORE\n");
    }
    printf("valore sem 0: %d  \n", semctl(id, 0, GETVAL, &arg));
    
    
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