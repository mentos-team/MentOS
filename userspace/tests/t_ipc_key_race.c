/// @file t_ipc_key_race.c
/// @brief Checks that concurrent key creation publishes one IPC object.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#define CHILDREN 8

static int create_object(int kind, key_t key)
{
    if (kind == 0) {
        return msgget(key, IPC_CREAT | 0600);
    }
    if (kind == 1) {
        return semget(key, 1, IPC_CREAT | 0600);
    }
    return shmget(key, 4096, IPC_CREAT | 0600);
}

static int remove_object(int kind, int id)
{
    if (kind == 0) {
        return msgctl(id, IPC_RMID, NULL);
    }
    if (kind == 1) {
        return semctl(id, 0, IPC_RMID, NULL);
    }
    return shmctl(id, IPC_RMID, NULL);
}

static int run_key_race(int kind, key_t key)
{
    int channel[2];
    int ids[CHILDREN];
    if (pipe(channel) < 0) {
        return 1;
    }

    for (int i = 0; i < CHILDREN; ++i) {
        const pid_t child = fork();
        if (child == 0) {
            close(channel[0]);
            const int id = create_object(kind, key);
            const int result = write(channel[1], &id, sizeof(id)) == sizeof(id) ? EXIT_SUCCESS : EXIT_FAILURE;
            close(channel[1]);
            exit(result);
        }
        if (child < 0) {
            close(channel[0]);
            close(channel[1]);
            return 1;
        }
    }

    close(channel[1]);
    int failed = 0;
    for (int i = 0; i < CHILDREN; ++i) {
        if (read(channel[0], &ids[i], sizeof(ids[i])) != sizeof(ids[i])) {
            failed = 1;
        }
    }
    close(channel[0]);
    for (int i = 0; i < CHILDREN; ++i) {
        int status = 0;
        if (wait(&status) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
            failed = 1;
        }
    }

    if (!failed && ids[0] < 0) {
        failed = 1;
    }
    for (int i = 1; i < CHILDREN; ++i) {
        if (ids[i] != ids[0]) {
            failed = 1;
        }
    }
    if (ids[0] >= 0 && remove_object(kind, ids[0]) < 0) {
        failed = 1;
    }
    return failed;
}

int main(void)
{
    openlog("t_ipc_key_race", LOG_PID | LOG_CONS, LOG_USER);
    const int failed = run_key_race(0, 0x4D534751) || run_key_race(1, 0x53454D51) || run_key_race(2, 0x53484D51);
    syslog(failed ? LOG_ERR : LOG_INFO, "[t_ipc_key_race] %s", failed ? "key race checks failed" : "key race checks passed");
    closelog();
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
