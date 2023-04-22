/// @file ipcs.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <ipc/sem.h>
#include <ipc/ipc.h>
#include <stdlib.h>
#include <string.h>


#if 0
static inline void print_sem_stat()
{
    printf("Semaphores: \n");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s \n", "T", "ID", "KEY", "MODE", "OWNER", "GROUP");
    printf("%-20s %-10s %-20s \n\n", "", "Semaphores not implemented", "");
}

static inline void print_msg_stat()
{
    printf("Message Queues: \n");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s \n", "T", "ID", "KEY", "MODE", "OWNER", "GROUP");
    printf("%-20s %-10s %-20s \n\n", "", "Message Queues not implemented", "");
}

static inline void print_shm_stat()
{
    printf("Shared Memory: \n");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s \n", "T", "ID", "KEY", "MODE",           "OWNER", "GROUP");
    printf("%-20s %-10s %-20s \n\n", "", "Shared Memory not implemented", "");
}
#endif

int main(int argc, char **argv)
{

    /*if (ipcs(argc, argv) == -1){
        printf("Errore\n");
    }*/

    if (argc>4)return -1;
    if (argc == 1){ //default
        long __res;
        __res = ipcs();
        return __res;
    }
    if (argc == 2){
        if (!strcmp(argv[1], "-s")){ //semaphores
            long __res;
            __res = ipcs();
            return __res;
        }

        if (!strcmp(argv[1], "-m")) { //shared memories
            printf("Not Implemented!\n");
            return 0;
        }

        if (!strcmp(argv[1], "-q")){ //message queues
            printf("Not Implemented!\n");
            return 0;
        }

        return -1;        
    }
    else{
        if(!strcmp(argv[1], "-i" ) && !strcmp(argv[3],"-s")){
            union semun temp;
            temp.buf = (struct semid_ds *)malloc(sizeof(struct semid_ds));
            long __res;
            __res = semctl(atoi(argv[2]), 0, GETNSEMS, NULL);
            //__inline_syscall4(__res, semctl, atoi(argv[2]), 0, GETNSEMS, NULL);
            temp.buf -> sems = (struct sem *)malloc(sizeof(struct sem) * __res);
            
            __res = semctl(atoi(argv[2]), 0, IPC_STAT, &temp);
            //__inline_syscall4(__res, semctl, atoi(argv[2]), 0, IPC_STAT, &temp);
            printf("%d\t\t%d\t\t%d\t\t\t%d\n", temp.buf->key, temp.buf->semid, temp.buf->owner, temp.buf->sem_nsems);
            if(__res == -1)
                return -1;
            return __res;
        }
        if(!strcmp(argv[1], "-i" ) && !strcmp(argv[3],"-m")){
            printf("Not Implemented!\n");
            return 0;
        }
        if(!strcmp(argv[1], "-i" ) && !strcmp(argv[3],"-q")){
            printf("Not Implemented!\n");
            return 0;
        }
        return -1;
    }


    return 0;



#if 0
    if (argc > 2) {
        printf("Too much arguments.\n");

        return;
    }

    char datehour[100] = "";
    strdatehour(datehour);

    printf("IPC status from " OS_NAME " as of %s\n", datehour);

    if (argc == 2) {
        if (strcmp(argv[1], "-s") == 0) {
            print_sem_stat();
        } else if (strcmp(argv[1], "-m") == 0) {
            print_shm_stat();
        } else if (strcmp(argv[1], "-q") == 0) {
            print_msg_stat();
        } else {
            printf("Option not recognize.\n");
        }
    } else {
        print_sem_stat();
        print_shm_stat();
        print_msg_stat();
    }

    return;
#endif
}
