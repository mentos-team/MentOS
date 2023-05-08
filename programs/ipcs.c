/// @file ipcs.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <stdio.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

static inline void __print_file_content(const char *path)
{
    // Prepare the buffer for reading.
    char buffer[BUFSIZ];
    // Clean the buffer.
    memset(buffer, 0, BUFSIZ);
    // Open the file.
    int fd = open(path, O_RDONLY, 42);
    if (fd >= 0) {
        // Put on the standard output the characters.
        while (read(fd, buffer, BUFSIZ) > 0)
            puts(buffer);
        // Close the file descriptor.
        close(fd);
    }
}

int main(int argc, char **argv)
{
    if (argc > 4)
        return -1;

    // Default operation, prints all ipcs informations.
    if (argc == 1) {
        printf("------ Message Queues --------\n");
        __print_file_content("/proc/ipc/msg");
        printf("------ Semaphore Arrays --------\n");
        __print_file_content("/proc/ipc/sem");
        printf("------ Shared Memory Segments --------\n");
        __print_file_content("/proc/ipc/shm");
        return 0;
    }
    if (argc == 2) {
        if (!strcmp(argv[1], "-q")) { // Show message queues information.
            printf("------ Message Queues --------\n");
            __print_file_content("/proc/ipc/msg");
        } else if (!strcmp(argv[1], "-s")) { // Show semaphores information.
            printf("------ Semaphore Arrays --------\n");
            __print_file_content("/proc/ipc/sem");
        } else if (!strcmp(argv[1], "-m")) { // Show shared memories information.
            printf("------ Shared Memory Segments --------\n");
            __print_file_content("/proc/ipc/shm");
        } else {
            printf("%s: Command not found.", argv[0]);
            return 1;
        }
        return 0;
    }
    if (!strcmp(argv[1], "-i")) {
        // Show details about a specific semaphore.
        if (!strcmp(argv[3], "-s")) {
            struct semid_ds sem;
            union semun temp;
            long ret;
            // Prepare the data structure.
            temp.buf = &sem;
            // Retrive the statistics.
            ret = semctl(atoi(argv[2]), 0, IPC_STAT, &temp);
            // Check if we succeded.
            if (!ret) {
                //ret          = semctl(atoi(argv[2]), 0, GETNSEMS, NULL);
                //temp.buf->sems = (struct sem *)malloc(sizeof(struct sem) * ret);
                printf("key        semid      owner      perms      nsems\n");
                printf("%10d %10d %10d %10d %d\n", sem.key, sem.semid, sem.owner, 0, sem.sem_nsems);
                return 0;
            }
            return 1;
        }

        // Show details about a specific shared memory.
        if (!strcmp(argv[3], "-m")) {
            printf("Not Implemented!\n");
            return 0;
        }

        // Show details about a specific message queue.
        if (!strcmp(argv[3], "-q")) {
            printf("Not Implemented!\n");
            return 0;
        }
        printf("%s: Wrong combination, with `-i` you should provide either `-s`, `-m`, or `-q`.", argv[0]);
        return 1;
    }
    printf("%s: Command not found.", argv[0]);
    return 1;
}
