//test
#include "ipc/sem.h"
#include "stdio.h"
#include "ipc/ipc.h"
#include "sys/errno.h"



int main()
{      
    union semun arg;
    arg.val = 150;    
    //key_t i = ftok("/test1.txt", 5);  //checking ftok
    long id = semget(2,2,0);  //first semaphores
    //long id2 = semget(IPC_PRIVATE, 1, IPC_CREAT);
    //long id2 = semget(2, 2, IPC_EXCL);  //need to return -1 bc already exists

    //long id3 = semget(2, 2, 0);  //need to return 3 (semid of the first)

    //long id4 = semget(100, 2, IPC_CREAT | IPC_EXCL);  //need to return 101 (new semaphore)
    long value;

    if ((value = semctl(id, 1, SETVAL, arg)) == -1){
        printf("%d \n", value);
    }else{
        printf("getpid: %d\n", value);
    }

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