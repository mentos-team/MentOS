/// @file t_ipc_lifetime.c
/// @brief Exercises IPC_RMID wakeups and message-queue tail maintenance.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

typedef struct {
    long type;
    char text[8];
} test_message_t;

static int test_message_tail(void)
{
    const int queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    test_message_t message = {0};
    char received[8] = {0};
    if (queue < 0) {
        return 1;
    }

    message.type = 1;
    memcpy(message.text, "one", 4);
    if (msgsnd(queue, &message, 4, 0) < 0) {
        return 1;
    }
    message.type = 2;
    memcpy(message.text, "two", 4);
    if (msgsnd(queue, &message, 4, 0) < 0) {
        return 1;
    }
    if (msgrcv(queue, &message, sizeof(message.text), 1, 0) != 4) {
        return 1;
    }
    message.type = 3;
    memcpy(message.text, "three", 6);
    if (msgsnd(queue, &message, 6, 0) < 0) {
        return 1;
    }
    if (msgrcv(queue, received, sizeof(received), 2, 0) != 4) {
        return 1;
    }
    if (msgrcv(queue, received, sizeof(received), 3, 0) != 6) {
        return 1;
    }
    return msgctl(queue, IPC_RMID, NULL) < 0;
}

static int test_semaphore_rmid_wakeup(void)
{
    int ready[2];
    struct sembuf operation = {.sem_num = 0, .sem_op = -1, .sem_flg = 0};
    const int semaphore = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (semaphore < 0 || pipe(ready) < 0) {
        return 1;
    }

    const pid_t child = fork();
    if (child == 0) {
        char marker = 'R';
        close(ready[0]);
        if (write(ready[1], &marker, 1) != 1 || semop(semaphore, &operation, 1) != -1 || errno != EIDRM) {
            exit(1);
        }
        exit(0);
    }
    if (child < 0) {
        return 1;
    }

    close(ready[1]);
    char marker = 0;
    const int signaled = read(ready[0], &marker, 1) == 1;
    close(ready[0]);
    const int removed = semctl(semaphore, 0, IPC_RMID, NULL) == 0;
    int status = 0;
    waitpid(child, &status, 0);
    return !signaled || !removed || !WIFEXITED(status) || WEXITSTATUS(status) != 0;
}

static int test_message_rmid_wakeup(void)
{
    int ready[2];
    test_message_t message = {.type = 1};
    const int queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (queue < 0 || pipe(ready) < 0) {
        return 1;
    }

    const pid_t child = fork();
    if (child == 0) {
        char marker = 'R';
        close(ready[0]);
        if (write(ready[1], &marker, 1) != 1 ||
            msgrcv(queue, &message, sizeof(message.text), 0, 0) != -1 || errno != EIDRM) {
            exit(1);
        }
        exit(0);
    }
    if (child < 0) {
        msgctl(queue, IPC_RMID, NULL);
        return 1;
    }

    close(ready[1]);
    char marker = 0;
    const int signaled = read(ready[0], &marker, 1) == 1;
    close(ready[0]);
    const int removed = msgctl(queue, IPC_RMID, NULL) == 0;
    int status = 0;
    waitpid(child, &status, 0);
    return !signaled || !removed || !WIFEXITED(status) || WEXITSTATUS(status) != 0;
}

static int test_message_sender_rmid_wakeup(void)
{
    int ready[2];
    const int queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (queue < 0 || pipe(ready) < 0) {
        return 1;
    }

    // MSGMNB is two MSGMAX messages in this implementation.  The child is
    // therefore guaranteed to block on its third send.
    struct {
        long type;
        char text[MSGMAX];
    } large = {.type = 2};
    if (msgsnd(queue, &large, MSGMAX, 0) < 0 || msgsnd(queue, &large, MSGMAX, 0) < 0) {
        msgctl(queue, IPC_RMID, NULL);
        return 1;
    }

    const pid_t child = fork();
    if (child == 0) {
        char marker = 'S';
        close(ready[0]);
        if (write(ready[1], &marker, 1) != 1 || msgsnd(queue, &large, MSGMAX, 0) != -1 || errno != EIDRM) {
            exit(1);
        }
        exit(0);
    }
    if (child < 0) {
        msgctl(queue, IPC_RMID, NULL);
        return 1;
    }

    close(ready[1]);
    char marker = 0;
    const int signaled = read(ready[0], &marker, 1) == 1;
    close(ready[0]);
    const int removed = msgctl(queue, IPC_RMID, NULL) == 0;
    int status = 0;
    waitpid(child, &status, 0);
    return !signaled || !removed || !WIFEXITED(status) || WEXITSTATUS(status) != 0;
}

static int test_shared_memory_rmid_mapping(void)
{
    const int shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    if (shmid < 0) {
        return 1;
    }
    char *mapping = (char *)shmat(shmid, NULL, 0);
    if (mapping == (char *)-1) {
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }
    struct shmid_ds metadata;
    if (shmctl(shmid, IPC_STAT, &metadata) < 0 || metadata.shm_segsz < 4096) {
        shmdt(mapping);
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    memcpy(mapping, "alive after rmid", 17);
    if (shmctl(shmid, IPC_RMID, NULL) < 0 || memcmp(mapping, "alive after rmid", 17) != 0) {
        shmdt(mapping);
        return 1;
    }
    return shmdt(mapping) < 0;
}

/// @brief Reject shared-memory sizes that would overflow page rounding.
static int test_shared_memory_size_bounds(void)
{
    const size_t too_large = (size_t)-1;
    errno = 0;
    if (shmget(IPC_PRIVATE, too_large, IPC_CREAT | 0600) != -1 || errno != EINVAL) {
        return 1;
    }
    return 0;
}

/// @brief Verify the stable errno contract for missing and removed objects.
static int test_missing_object_errors(void)
{
    const int missing_id = 0x7FFFFFFF;
    struct msqid_ds message_metadata;
    struct shmid_ds shared_metadata;
    struct sembuf operation = {.sem_num = 0, .sem_op = -1, .sem_flg = IPC_NOWAIT};
    test_message_t message = {.type = 1};
    union semun semaphore_argument = {.buf = NULL};

    errno = 0;
    if (msgctl(missing_id, IPC_STAT, &message_metadata) != -1 || errno != EIDRM) {
        return 1;
    }
    errno = 0;
    if (msgsnd(missing_id, &message, 0, IPC_NOWAIT) != -1 || errno != EIDRM) {
        return 1;
    }
    errno = 0;
    if (msgrcv(missing_id, &message, sizeof(message.text), 0, IPC_NOWAIT) != -1 || errno != EIDRM) {
        return 1;
    }

    errno = 0;
    if (semctl(missing_id, 0, IPC_STAT, &semaphore_argument) != -1 || errno != EINVAL) {
        return 1;
    }
    errno = 0;
    if (semop(missing_id, &operation, 1) != -1 || errno != EINVAL) {
        return 1;
    }

    errno = 0;
    if (shmctl(missing_id, IPC_STAT, &shared_metadata) != -1 || errno != EINVAL) {
        return 1;
    }
    errno = 0;
    if (shmat(missing_id, NULL, 0) != (void *)-1 || errno != ENOENT) {
        return 1;
    }

    const int queue = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    const int semaphore = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    const int shared = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    if (queue < 0 || semaphore < 0 || shared < 0) {
        return 1;
    }
    if (msgctl(queue, IPC_RMID, NULL) < 0 || semctl(semaphore, 0, IPC_RMID, NULL) < 0 ||
        shmctl(shared, IPC_RMID, NULL) < 0) {
        return 1;
    }

    errno = 0;
    if (msgctl(queue, IPC_STAT, &message_metadata) != -1 || errno != EIDRM) {
        return 1;
    }
    errno = 0;
    if (semctl(semaphore, 0, IPC_STAT, &semaphore_argument) != -1 || errno != EINVAL) {
        return 1;
    }
    errno = 0;
    if (shmctl(shared, IPC_STAT, &shared_metadata) != -1 || errno != EINVAL) {
        return 1;
    }
    return 0;
}

int main(void)
{
    openlog("t_ipc_lifetime", LOG_PID | LOG_CONS, LOG_USER);
    const int failed = test_message_tail() || test_message_rmid_wakeup() || test_message_sender_rmid_wakeup() ||
                       test_semaphore_rmid_wakeup() || test_shared_memory_rmid_mapping() || test_shared_memory_size_bounds() ||
                       test_missing_object_errors();
    syslog(failed ? LOG_ERR : LOG_INFO, "[t_ipc_lifetime] %s", failed ? "lifetime checks failed" : "lifetime checks passed");
    closelog();
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
