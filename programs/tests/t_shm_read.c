#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main(int argc, char *argv[])
{
    key_t key;
    int shmid, id;
    char *str, *ptr;
    char *path;
    if (argc != 3) {
        printf("%s: You must provide a file and the id to generate the key.\n", argv[0]);
        return 1;
    }
    // Get the file.
    path = argv[1];
    // Get the id.
    id = strtol(argv[2], &ptr, 10);
    // Generate the key.
    key = ftok(path, id);
    if (key == -1) {
        perror("ftok");
        return EXIT_FAILURE;
    }
    printf("id = %d; key = %d\n", id, key);
    // Create shared memory.
    shmid = shmget(key, 1024, 0666);
    if (shmid == -1) {
        perror("shmget");
        return EXIT_FAILURE;
    }
    printf("shmid = %d;\n", shmid);
    // Attach the shared memory.
    str = (char *)shmat(shmid, NULL, SHM_RDONLY);
    if (str == NULL) {
        perror("shmat");
        return EXIT_FAILURE;
    }
    // Print the message.
    printf("Data read from memory: %s (%p)\n", str, str);
    strcpy(str, "Hello!");
    // printf("Data read from memory: %s (%p)\n", str, str);
    // Detatch the shared memory.
    if (shmdt(str) < 0) {
        perror("shmdt");
        return EXIT_FAILURE;
    }
    // Remove the shared memory.
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        return EXIT_FAILURE;
    }
    printf("Exiting.\n");
    return EXIT_SUCCESS;
}
