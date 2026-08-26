/// @file t_semget.c
/// @brief This program demonstrates semaphore operations between a parent and
/// child process. The child process increments a semaphore after sleeping,
/// which unblocks the parent process. Finally, the semaphore is deleted by the
/// child process before exiting.
/// @copyright (c) 2014-2024
/// This file is distributed under the MIT License. See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    struct sembuf op_child;     // Semaphore operation for the child.
    struct sembuf op_father[3]; // Semaphore operations for the father.
    union semun arg;            // Union for semctl operations.
    long ret;
    long semid; // Return values and semaphore ID.
    key_t key;  // Key for semaphore.

    // ========================================================================
    // Generate a unique key using ftok.
    key = ftok("/", 5);
    if (key < 0) {
        syslog(LOG_ERR, "[t_semget] [t_semget] Failed to generate key using ftok: %s\n", strerror(errno));
        return 1;
    }
    syslog(LOG_INFO, "[t_semget] [t_semget] Generated key using ftok (key = %d)\n", key);

    // ========================================================================
    // Create a semaphore set with one semaphore.
    semid = semget(key, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (semid < 0) {
        syslog(LOG_ERR, "[t_semget] [t_semget] Failed to create semaphore set: %s\n", strerror(errno));
        return 1;
    }
    syslog(LOG_INFO, "[t_semget] [t_semget][father] Created semaphore set (id : %ld)\n", semid);

    // ========================================================================
    // Set the value of the semaphore to 1.
    arg.val = 1;
    ret     = semctl(semid, 0, SETVAL, &arg);
    if (ret < 0) {
        syslog(LOG_ERR, "[t_semget] [t_semget] Failed to set semaphore value: %s\n", strerror(errno));
        return 1;
    }
    syslog(LOG_INFO, "[t_semget] [t_semget][father] Set semaphore value to 1 (id : %ld)\n", semid);

    // ========================================================================
    // Verify that the semaphore value is set correctly.
    ret = semctl(semid, 0, GETVAL, NULL);
    if (ret < 0) {
        syslog(LOG_ERR, "[t_semget] [t_semget] Failed to get semaphore value: %s\n", strerror(errno));
        return 1;
    }
    syslog(LOG_INFO, "[t_semget] [t_semget][father] Semaphore value is %ld (expected: 1)\n", ret);

    // ========================================================================
    // Fork a child process.
    if (fork() == 0) {
        // Child process setup.
        op_child.sem_num = 0; // Operate on semaphore 0.
        op_child.sem_op  = 1; // Increment semaphore by 1.
        op_child.sem_flg = 0; // No special flags.

        // Simulate some work before modifying the semaphore.
        timespec_t req = {0, 200000000};
        nanosleep(&req, NULL);

        // Increment the semaphore, unblocking the parent.
        if (semop(semid, &op_child, 1) < 0) {
            syslog(LOG_ERR, "[t_semget] [t_semget][child] Failed to perform child semaphore operation: %s\n", strerror(errno));
            return 1;
        }
        syslog(LOG_INFO, "[t_semget] [t_semget][child] Performed first semaphore operation (id: %ld)\n", semid);

        // Verify the updated semaphore value.
        ret = semctl(semid, 0, GETVAL, NULL);
        if (ret < 0) {
            syslog(LOG_ERR, "[t_semget] [t_semget][child] Failed to get semaphore value in child: %s\n", strerror(errno));
            return 1;
        }
        syslog(LOG_INFO, "[t_semget] [t_semget][child] Semaphore value after first increment is %ld (concurrent parent may consume immediately)\n", ret);

        // Sleep and perform another increment operation.
        nanosleep(&req, NULL);
        if (semop(semid, &op_child, 1) < 0) {
            syslog(LOG_ERR, "[t_semget] [t_semget][child] Failed to perform second child semaphore operation: %s\n", strerror(errno));
            return 1;
        }
        syslog(LOG_INFO, "[t_semget] [t_semget][child] Performed second semaphore operation (id: %ld)\n", semid);

        // Check final semaphore value.
        ret = semctl(semid, 0, GETVAL, NULL);
        if (ret < 0) {
            syslog(LOG_ERR, "[t_semget] [t_semget][child] Failed to get final semaphore value in child: %s\n", strerror(errno));
            return 1;
        }
        syslog(LOG_INFO, "[t_semget] [t_semget][child] Final semaphore value is %ld\n", ret);

        // Exit the child process. The semaphore set is deliberately NOT
        // removed here: the father must verify the final value and reap the
        // child before the set is deleted, otherwise its GETVAL races with
        // this IPC_RMID and fails with EINVAL (#255).
        return 0;
    }

    // ========================================================================
    // Parent process: Prepare operations to decrement semaphore.
    op_father[0].sem_num = 0;  // Operate on semaphore 0.
    op_father[0].sem_op  = -1; // Decrement by 1.
    op_father[0].sem_flg = 0;  // No special flags.

    op_father[1].sem_num = 0;  // Operate on semaphore 0.
    op_father[1].sem_op  = -1; // Decrement by 1.
    op_father[1].sem_flg = 0;  // No special flags.

    op_father[2].sem_num = 0;  // Operate on semaphore 0.
    op_father[2].sem_op  = -1; // Decrement by 1.
    op_father[2].sem_flg = 0;  // No special flags.

    // ========================================================================
    // Perform the blocking semaphore operations.
    if (semop(semid, op_father, 3) < 0) {
        syslog(LOG_ERR, "[t_semget] [t_semget][father] Failed to perform parent semaphore operations: %s\n", strerror(errno));
        semctl(semid, 0, IPC_RMID, 0);
        return 1;
    }
    syslog(LOG_INFO, "[t_semget] [t_semget][father] Performed semaphore operations (id: %ld)\n", semid);

    // Verify that the semaphore value is updated correctly. This runs after
    // the blocking semop returned, which can only happen once the child
    // performed both increments, so the value is deterministically 0 and
    // the set is guaranteed to still exist (the child no longer removes it).
    ret = semctl(semid, 0, GETVAL, NULL);
    if (ret < 0) {
        syslog(LOG_ERR, "[t_semget] [t_semget][father] Failed to get semaphore value in parent: %s\n", strerror(errno));
        semctl(semid, 0, IPC_RMID, 0);
        return 1;
    }
    syslog(LOG_INFO, "[t_semget] [t_semget][father] Semaphore value is %ld (expected: 0)\n", ret);

    // Wait for the child process to terminate.
    if (wait(NULL) == -1) {
        syslog(LOG_ERR, "[t_semget] [t_semget] Failed to wait for child process: %s\n", strerror(errno));
        semctl(semid, 0, IPC_RMID, 0);
        return EXIT_FAILURE; // Return failure if wait fails.
    }

    // Delete the semaphore set. Doing this in the father, after the child
    // has been reaped and the final value verified, removes the race where
    // the child's IPC_RMID invalidated the id while the father was still
    // using it (#255). Keeping the cleanup on the father's error paths also
    // prevents a failed run from leaving a stale set behind and breaking
    // the next run's IPC_CREAT | IPC_EXCL.
    ret = semctl(semid, 0, IPC_RMID, 0);
    if (ret < 0) {
        syslog(LOG_ERR, "[t_semget] [t_semget][father] Failed to remove semaphore set: %s\n", strerror(errno));
        return 1;
    }
    syslog(LOG_INFO, "[t_semget] [t_semget][father] Removed semaphore set (id: %ld)\n", semid);

    return 0;
}

