//test
#include "sys/sem.h"
#include "stdio.h"
#include "sys/ipc.h"
#include "sys/errno.h"
#include "sys/wait.h"
#include "stdlib.h"
#include "sys/unistd.h"


/*
First test of System V Semaphores.
Correct output should be: Corso di Sistemi Operativi.
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

	struct sembuf sops[6];
	sops[0].sem_num = 0;
	sops[0].sem_op = -1;
	sops[0].sem_flg = 0;

	sops[1].sem_num = 1;
	sops[1].sem_op = -1;
	sops[1].sem_flg = 0;

	sops[2].sem_num = 2;
	sops[2].sem_op = -1;
	sops[2].sem_flg = 0;

	sops[3].sem_num = 0;
	sops[3].sem_op = 1;
	sops[3].sem_flg = 0;

	sops[4].sem_num = 1;
	sops[4].sem_op = 1;
	sops[4].sem_flg = 0;

	sops[5].sem_num = 2;
	sops[5].sem_op = 1;
	sops[5].sem_flg = 0;

	int status;
	//create a semaphore set
	int semid = semget(IPC_PRIVATE, 4, IPC_CREAT);
	//set the values of the semaphores 
	unsigned short  values[] = {0,0,0,1};
	arg.array = values;
	if(semctl(semid,0,SETALL,&arg)==-1)
		write(1,"errore set dei semafori",24);
	
	if(!fork()){
		semop(semid, &sops[0], 1);
		write(1,"Operativi.\n", 11);
		exit(0);
	}
	if(!fork()){
		semop(semid, &sops[1], 1);
		write(1,"Sistemi ", 8);
		semop(semid, &sops[3], 1);
		exit(0);
	}
	if(!fork()){
		semop(semid, &sops[2], 1);
		write(1," di ", 4);
		semop(semid, &sops[4], 1);
		exit(0);
	}
	if(!fork()){
		write(1,"Corso", 5);
		semop(semid, &sops[5], 1);
		exit(0);
	}
	for(int n=0; n<4; n++){
		wait(&status);
	}

	if(semctl(semid,0,IPC_RMID,0)==-1)
		write(1,"err", 3);
    return 0;
}