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

    if (argc>4)
        return -1;

    if (argc == 1){ /*Default operation, prints all ipcs informations*/
        long __res;
        __res = ipcs();
        return __res;
    }

    if (argc == 2){
        // all semaphores
        if (!strcmp(argv[1], "-s")){ 
            long __res;
            __res = semipcs();
            return __res;
        }

        // all shared memories
        if (!strcmp(argv[1], "-m")) { 
            printf("Not Implemented!\n");
            return 0;
        }

        // all message queues
        if (!strcmp(argv[1], "-q")){ 
            printf("Not Implemented!\n");
            return 0;
        }

        return -1;        
    }
    else{
        // semaphore by id
        if(!strcmp(argv[1], "-i" ) && !strcmp(argv[3],"-s")){
            union semun temp;
            temp.buf = (struct semid_ds *)malloc(sizeof(struct semid_ds));
            long __res;
            __res = semctl(atoi(argv[2]), 0, GETNSEMS, NULL);
            temp.buf -> sems = (struct sem *)malloc(sizeof(struct sem) * __res);
            __res = semctl(atoi(argv[2]), 0, IPC_STAT, &temp);
            printf("------ Matrici semafori --------\n");
            printf("chiave    semid    proprietario    nsems\n");
            printf("%d         %d        %d               %d\n", temp.buf->key, temp.buf->semid, temp.buf->owner, temp.buf->sem_nsems);
            if(__res == -1)
                return -1;
            return __res;
        }

        // shared memory by id
        if(!strcmp(argv[1], "-i" ) && !strcmp(argv[3],"-m")){
            printf("Not Implemented!\n");
            return 0;
        }

        // message queue by id
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
