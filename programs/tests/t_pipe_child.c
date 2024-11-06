/// @file t_pipe_child.c
/// @brief Test pipe communication between parent and child processes.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// ============================================================================
// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TEST  ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.
// ============================================================================

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strerror.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>

int main(int argc, char *argv[])
{
    int fds[2];
    const char write_msg[] =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n"
        "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.\n"
        "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris\n"
        "nisi ut aliquip ex ea commodo consequat.";
    char read_msg[sizeof(write_msg)];
    size_t total_messages = 3;

    pr_info("\n\nStarting parent-child pipe test.\n");

    // Create a pipe.
    if (pipe(fds) == -1) {
        pr_err("Failed to create pipe.\n");
        return 1;
    }
    pr_info("Pipe created: fds[0] = %d (read), fds[1] = %d (write).\n", fds[0], fds[1]);

    // Create a semaphore set with two semaphores
    int sem_id = semget(IPC_PRIVATE, 2, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("Failed to create semaphores");
        return 1;
    }

    // Initialize the semaphores
    union semun arg;
    arg.val = 0; // Initial value for parent_write_done semaphore
    if (semctl(sem_id, 0, SETVAL, &arg) == -1) {
        perror("Failed to initialize parent_write_done semaphore");
        return 1;
    }
    arg.val = 1; // Initial value for child_read_done semaphore
    if (semctl(sem_id, 1, SETVAL, &arg) == -1) {
        perror("Failed to initialize child_read_done semaphore");
        return 1;
    }

    // Fork a child process.
    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed.
        pr_err("Fork failed.\n");
        return 1;
    }

    if (pid > 0) {
        // Parent process.
        pr_info("In parent process (PID: %d).\n", getpid());

        // Close the read end of the pipe in the parent.
        if (close(fds[0]) == -1) {
            pr_err("Parent: Failed to close the read end of the pipe: %s\n", strerror(errno));
        } else {
            pr_debug("Parent closed the read end of the pipe.\n");
        }

        struct sembuf sem_wait_child    = { 1, -1, 0 }; // Wait on child_read_done
        struct sembuf sem_signal_parent = { 0, 1, 0 };  // Signal parent_write_done

        // Write multiple messages to the pipe.
        for (size_t i = 0; i < total_messages; i++) {
            // Wait for the child to finish reading before writing.
            semop(sem_id, &sem_wait_child, 1);

            pr_info("\nParent writing into pipe...\n");
            if (write(fds[1], write_msg, strlen(write_msg)) == -1) {
                pr_err("Parent write to pipe failed (%s).\n", strerror(errno));
                close(fds[1]);
                return 1;
            }
            pr_info("Parent successfully wrote to pipe (%u of %u):"
                    "\n----------------------------------------\n"
                    "%s"
                    "\n----------------------------------------\n\n",
                    i, total_messages, write_msg);

            // Signal the child that the parent has finished writing.
            semop(sem_id, &sem_signal_parent, 1);
        }

        // Close the write end after writing.
        close(fds[1]);
        pr_info("Parent closed the write end of the pipe.\n");

        // Wait for the child to finish.
        wait(NULL);
        pr_info("Parent process completed.\n");

        // Cleanup semaphores
        if (semctl(sem_id, 0, IPC_RMID, &arg) == -1) {
            perror("Failed to remove semaphores");
            return 1;
        }
    } else {
        // Sleep for one second.
        sleep(1);

        // Child process.
        pr_info("In child process (PID: %d).\n", getpid());

        // Close the write end of the pipe in the child.
        if (close(fds[1]) == -1) {
            pr_err("Child: Failed to close the write end of the pipe: %s\n", strerror(errno));
        } else {
            pr_debug("Child closed the write end of the pipe.\n");
        }

        struct sembuf sem_wait_parent  = { 0, -1, 0 }; // Wait on parent_write_done
        struct sembuf sem_signal_child = { 1, 1, 0 };  // Signal child_read_done

        ssize_t bytes_read;
        for (size_t i = 0; i < total_messages; i++) {
            // Wait for the parent to finish writing before reading
            semop(sem_id, &sem_wait_parent, 1);

            pr_info("\nChild reading from pipe...\n");
            bytes_read = read(fds[0], read_msg, sizeof(read_msg) - 1);
            if (bytes_read < 0) {
                pr_err("Child: Read from pipe failed (%s).\n", strerror(errno));
                break;
            }

            // Null-terminate the read message and print it
            read_msg[bytes_read] = '\0';

            pr_info("Child successfully read from pipe (%u of %u):"
                    "\n----------------------------------------\n"
                    "%s"
                    "\n----------------------------------------\n\n",
                    i, total_messages, read_msg);

            // Signal the parent that the child has finished reading
            semop(sem_id, &sem_signal_child, 1);
        }

        // Close the read end.
        if (close(fds[0]) == -1) {
            pr_err("Child: Failed to close the read end of the pipe: %s\n", strerror(errno));
        } else {
            pr_debug("Child closed the read end of the pipe.\n");
        }
    }

    pr_info("Parent-child pipe test completed.\n\n");
    return 0;
}
