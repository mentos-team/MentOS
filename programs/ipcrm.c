/// @file ipcrm.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <string.h>
#include <ipc/sem.h>
#include <ipc/ipc.h>
#include <stdlib.h>


int main(int argc, char **argv)
{
    /*Checking for arguments*/
    if (argc < 3){
        printf("Missing arguments...\n");
        return 0;
    }

    if (argc == 3){
        if (!strcmp("-a", argv[1]) || !strcmp("--all", argv[1])){
            printf("To be implemented...\n");
            return 0;
        }

        if (!strcmp("-M", argv[1])){
            printf("To be implemented...\n");
            return 0;
        }

        if (!strcmp("-m", argv[1])){
            printf("To be implemented...\n");
            return 0;
        }

        if (!strcmp("-Q", argv[1])){
            printf("To be implemented...\n");
            return 0;
        }

        if (!strcmp("-q", argv[1])){
            printf("To be implemented...\n");
            return 0;
        }

        /* Remove the semaphore set with the given key. */
        if (!strcmp("-S", argv[1])){

            int semid;

            if((semid = semget(atoi(argv[2]), 0, 0)) == -1){
                printf("There is no semaphores set with this key...\n");
                return -1;
            }
            if (semctl(semid, 0, IPC_RMID, NULL) == -1){
                printf("There is no semaphore set with this sem_id...\n");
                return -1;
            }

            printf("Done!\n");

            return 0;
            
        }

        /* Remove the semaphore set with the given id. */
        if (!strcmp("-s", argv[1])){

            int semid = atoi(argv[2]);

            if (semctl(semid, 0, IPC_RMID, NULL) == -1){
                printf("There is no semaphore set with this sem_id...\n");
                return -1;
            }
            
            printf("Done!\n");
            
            return 0;
        }

        printf("Unknown argument...\n");
    }

    return 0;

#if 0
    if (argc != 2) {
        printf("Bad arguments: you have to specify only IPC id, see ipcs.\n");

        return;
    }

    struct shmid_ds *shmid_ds = head;
    struct shmid_ds *prev = NULL;

    while (shmid_ds != NULL) {
        char strid[10];
        int_to_str(strid, (shmid_ds->shm_perm).seq, 10);

        if (strcmp(strid, argv[1]) == 0) {
            break;
        }

        prev = shmid_ds;
        shmid_ds = shmid_ds->next;
    }

    if (shmid_ds == NULL) {
        printf("No shared memory find. \n");
    } else {
        kfree(shmid_ds->shm_location);

        // shmid_ds = head.
        if (prev == NULL) {
            head = head->next;
        } else {
            prev->next = shmid_ds->next;
        }

        kfree(shmid_ds);
    }
#endif
}
