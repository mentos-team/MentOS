///                MentOS, The Mentoring Operating system project
/// @file cmd_ipcrm.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stdio.h"
#include "shm.h"
#include "string.h"

extern struct shmid_ds *head;

void cmd_ipcrm(int argc, char **argv){
    if (argc != 2)
    {
        printf("Bad arguments: you have to specify only IPC id, see ipcs.\n");

        return;
    }

    struct shmid_ds *shmid_ds = head;
    struct shmid_ds *prev = NULL;

    while (shmid_ds != NULL)
    {
        char strid[10];
        int_to_str(strid, (shmid_ds ->shm_perm).seq, 10);

        if (strcmp(strid, argv[1]) == 0)
        {
            break;
        }

        prev = shmid_ds;
        shmid_ds = shmid_ds -> next;
    }

    if (shmid_ds == NULL)
    {
        printf("No shared memory find. \n");
    }
    else
    {
        kfree(shmid_ds -> shm_location);

        // shmid_ds = head.
        if (prev == NULL)
        {
            head = head -> next;
        }
        else
        {
            prev -> next = shmid_ds -> next;
        }

        kfree(shmid_ds);
    }
}
