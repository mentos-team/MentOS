/// @file t_shm_read.c
/// @brief A program that reads data from a shared memory segment using a key
/// generated from a file and an id.
/// @copyright (c) 2014-2024
/// This file is distributed under the MIT License. See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    key_t key;
    int shmid, id;
    char *str, *ptr;
    ;
    char *path;

    // Ensure the correct number of command-line arguments are provided.
    if (argc != 3) {
        fprintf(stderr, "%s: You must provide a file and the id to generate the key.\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Get the file path and the id from the command-line arguments.
    path = argv[1];
    id   = strtol(argv[2], &ptr, 10);
    // Check if the conversion was successful and the number is non-negative.
    if (*ptr != '\0' || id < 0) {
        fprintf(STDERR_FILENO, "Invalid number: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    // Generate the IPC key using ftok with the provided file and id.
    key = ftok(path, id);
    if (key == -1) {
        perror("ftok");
        return EXIT_FAILURE;
    }
    printf("id = %d; key = %d\n", id, key);

    // Create or locate a shared memory segment based on the key. The size is
    // set to 1024 bytes, and 0666 allows read/write permissions.
    shmid = shmget(key, 1024, 0666);
    if (shmid == -1) {
        perror("shmget");
        return EXIT_FAILURE;
    }
    printf("shmid = %d;\n", shmid);

    // Attach the process to the shared memory segment in read-only mode (SHM_RDONLY).
    str = (char *)shmat(shmid, NULL, SHM_RDONLY);
    if (str == (char *)-1) {
        perror("shmat");
        return EXIT_FAILURE;
    }

    printf("Data read from memory: %s (%p)\n", str, str);

    // Attempting to modify shared memory here would fail due to SHM_RDONLY mode.
    // The following line is commented out as it will cause an error:
    // str[0] = 'H'; // Would trigger a memory access violation

    // Detach the process from the shared memory segment after use.
    if (shmdt(str) < 0) {
        perror("shmdt");
        return EXIT_FAILURE;
    }

    // Mark the shared memory segment for removal (IPC_RMID).
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        return EXIT_FAILURE;
    }

    printf("Exiting.\n");
    return EXIT_SUCCESS; // Return success
}
