/// @file 03_pipes.c
/// @brief Third example: Inter-process communication with pipes
/// @details This program demonstrates:
/// - Creating pipes for IPC
/// - Parent-child communication through pipes
/// - Read and write operations on pipe file descriptors
/// - Closing unused pipe ends
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int pipefd[2];
    char buffer[256];
    const char *message = "Hello from parent!";

    // Create a pipe: pipefd[0] is read end, pipefd[1] is write end
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    printf("Main: Created pipe\n");

    pid_t child_pid = fork();

    if (child_pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        // Child process: read from pipe
        printf("  Child: Closing write end of pipe\n");
        close(pipefd[1]); // Child doesn't write

        printf("  Child: Waiting to read from pipe...\n");
        ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate
            printf("  Child: Received message: %s\n", buffer);
        }

        close(pipefd[0]); // Done reading
        return EXIT_SUCCESS;
    } else {
        // Parent process: write to pipe
        printf("Main: Closing read end of pipe\n");
        close(pipefd[0]); // Parent doesn't read

        printf("Main: Writing message to pipe\n");
        write(pipefd[1], message, strlen(message));
        close(pipefd[1]); // Done writing

        printf("Main: Waiting for child...\n");
        waitpid(child_pid, NULL, 0);
        printf("Main: All done!\n");
        return EXIT_SUCCESS;
    }
}
