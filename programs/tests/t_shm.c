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

int shm_write(void)
{
    key_t key;
    int shmid;
    char *str;

    const char *message = "Hello there!\n";
    const char *path    = "/home";
    const int id        = 42;

    // Generate a System V IPC key using the predefined file path and id.
    key = ftok(path, id);
    if (key == -1) {
        fprintf(stderr, "Failed to generate IPC key using ftok.\n");
        return EXIT_FAILURE;
    }

    // Create a shared memory segment with the generated key, size of 1024 bytes,
    // and permissions 0666.
    shmid = shmget(key, 1024, IPC_CREAT | 0666);
    if (shmid == -1) {
        fprintf(stderr, "Failed to create shared memory segment using shmget.\n");
        return EXIT_FAILURE;
    }

    // Attach the shared memory segment to the process's address space.
    str = (char *)shmat(shmid, NULL, 0);
    if (str == (char *)-1) {
        fprintf(stderr, "Failed to attach shared memory segment using shmat.\n");
        return EXIT_FAILURE;
    }

    // Write a message to the shared memory, ensuring no buffer overflow.
    strncpy(str, message, strlen(message));
    str[1023] = '\0'; // Ensure null termination.

    // Detach the shared memory segment from the process's address space.
    if (shmdt(str) < 0) {
        fprintf(stderr, "Failed to detach shared memory segment using shmdt.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int shm_read(void)
{
    key_t key;
    int shmid;
    char *str;

    const char *message = "Hello there!\n";
    const char *path    = "/home";
    const int id        = 42;

    // Generate the IPC key using ftok with the predefined file and id.
    key = ftok(path, id);
    if (key == -1) {
        fprintf(stderr, "Failed to generate IPC key using ftok.\n");
        return EXIT_FAILURE;
    }

    // Create or locate a shared memory segment based on the key.
    // Size is set to 1024 bytes, and 0666 allows read/write permissions.
    shmid = shmget(key, 1024, 0666);
    if (shmid == -1) {
        fprintf(stderr, "Failed to create or access shared memory using shmget.\n");
        return EXIT_FAILURE;
    }

    // Attach the process to the shared memory segment in read-only mode (SHM_RDONLY).
    str = (char *)shmat(shmid, NULL, SHM_RDONLY);
    if (str == (char *)-1) {
        fprintf(stderr, "Failed to attach to shared memory segment using shmat.\n");
        return EXIT_FAILURE;
    }

    // Check if both hashes match.
    if (strcmp(str, message)) {
        fprintf(stderr, "Data does not match.\n");
        return EXIT_FAILURE;
    }

    // Detach the process from the shared memory segment after use.
    if (shmdt(str) < 0) {
        fprintf(stderr, "Failed to detach shared memory segment using shmdt.\n");
        return EXIT_FAILURE;
    }

    // Mark the shared memory segment for removal (IPC_RMID).
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        fprintf(stderr, "Failed to mark shared memory segment for removal using shmctl.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    if (shm_write()) {
        return EXIT_FAILURE;
    }
    if (shm_read()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
