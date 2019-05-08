///                MentOS, The Mentoring Operating system project
/// @file cmd_ipcs.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "shm.h"
#include "stdio.h"
#include "string.h"
#include "version.h"

extern struct shmid_ds *head;

static void print_sem_stat()
{
    printf("Semaphores: \n");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s \n", "T", "ID", "KEY", "MODE",
           "OWNER", "GROUP");
    printf("%-20s %-10s %-20s \n\n", "", "Semaphores not implemented", "");
}

static void print_shm_stat()
{
    struct shmid_ds *shm_list = head;

    printf("Shared Memory: \n");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s \n", "T", "ID", "KEY", "MODE",
           "OWNER", "GROUP");

    while (shm_list != NULL)
    {
        char mode[12];
        strmode((shm_list->shm_perm).mode, mode);

        printf("%-10s %-10i %-10i %-10s %-10s %-10s \n",
               "m",
               (shm_list->shm_perm).seq,
               (shm_list->shm_perm).key,
               mode,
               "-", "-");

        shm_list = shm_list->next;
    }

    printf("\n");
}

static void print_msg_stat()
{
    printf("Message Queues: \n");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s \n", "T", "ID", "KEY", "MODE",
           "OWNER", "GROUP");
    printf("%-20s %-10s %-20s \n\n", "", "Message Queues not implemented", "");
}

void cmd_ipcs(int argc, char **argv)
{
    if (argc > 2)
    {
        printf("Too much arguments.\n");

        return;
    }

    char datehour[100] = "";
    strdatehour(datehour);

    printf("IPC status from "OS_NAME" as of %s\n", datehour);

    if (argc == 2)
    {
        if (strcmp(argv[1], "-s") == 0)
        {
            print_sem_stat();
        }
        else if (strcmp(argv[1], "-m") == 0)
        {
            print_shm_stat();
        }
        else if (strcmp(argv[1], "-q") == 0)
        {
            print_msg_stat();
        }
        else
        {
            printf("Option not recognize.\n");
        }
    }
    else
    {
        print_sem_stat();
        print_shm_stat();
        print_msg_stat();
    }

    return;
}
