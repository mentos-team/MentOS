/// @file t_shm_write.c
/// @brief A program that writes data to a shared memory segment using a key
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
    char *path;

    // Ensure the correct number of command-line arguments are provided.
    if (argc != 3) {
        fprintf(stderr, "%s: You must provide a file and the id to generate the key.\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Get the file path and the id from command-line arguments.
    path = argv[1];
    id   = strtol(argv[2], &ptr, 10);

    // Validate that the provided id is a valid integer.
    if (*ptr != '\0') {
        fprintf(stderr, "%s: Invalid id: '%s'. Please provide a valid integer.\n", argv[0], argv[2]);
        return EXIT_FAILURE;
    }

    // Generate a System V IPC key using the provided file path and id.
    key = ftok(path, id);
    if (key == -1) {
        perror("ftok");
        return EXIT_FAILURE;
    }

    // Create a shared memory segment with the generated key, size of 1024
    // bytes, and permissions 0666.
    shmid = shmget(key, 1024, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        return EXIT_FAILURE;
    }

    // Attach the shared memory segment to the process's address space.
    str = (char *)shmat(shmid, NULL, 0);
    // Ensure the shared memory was attached correctly.
    if (str == (char *)-1) {
        perror("shmat");
        return EXIT_FAILURE;
    }

    // Write a message to the shared memory, ensuring no buffer overflow using strncpy.
    const char *message = "Hello there!\n";
    // Copy the message to shared memory.
    strncpy(str, message, 1024);
    // Ensure the string is null-terminated.
    str[1023] = '\0';

    // Detach the shared memory segment from the process's address space.
    if (shmdt(str) < 0) {
        perror("shmdt");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
