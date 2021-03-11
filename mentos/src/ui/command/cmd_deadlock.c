///                MentOS, The Mentoring Operating system project
/// @file cmd_deadlock.c
/// @brief Source file of deadlock shell command to test deadlock behavior with tasks and shared resources.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "smart_sem.h"
#include "wait.h"

#define DEFAULT_ITER "1"

/// @brief First concurrent task on resource r1 and r2.
static int _deadlock_task1(int argc, char **argv, char **envp);
/// @brief Second concurrent task on resource r1 and r2.
static int _deadlock_task2(int argc, char **argv, char **envp);

/// @brief Mutex semaphores that manage the resources access.
int mutex_r1, mutex_r2;
/// @brief Resources accessed by tasks.
uint32_t r1 = 0, r2 = 0;

static int _deadlock_task1(int argc, char **argv, char **envp)
{
    (void) envp;
    pid_t cpid2;

    size_t iter = (size_t) atoi(argc > 1 ? argv[1] : DEFAULT_ITER);

    if ((cpid2 = vfork()) == 0)
    {
        char *_argv[] = {"_deadlock_task2", argc > 1 ? argv[1] : DEFAULT_ITER, (char *) NULL};
        char *_envp[] = {(char *) NULL};

        execve((const char *) _deadlock_task2, _argv, _envp);

        printf("cmd_deadlock should not arrive here\n");
        return 0;
    }

    for (size_t i = 0; i < iter; i++)
    {
        sem_acquire(mutex_r1);
        sem_acquire(mutex_r2); //< DEADLOCK!

        // Access shared resources.
        uint32_t tmp = r1;
        r1 = r2;
        r2 = tmp;

        r1++;

        printf("[T1] { r1: %4i, r2: %4i }\n", r1, r2);

        sem_release(mutex_r2);
        sem_release(mutex_r1);
    }

    int status;
    waitpid(cpid2, &status, 0);
    return 0;
}

static int _deadlock_task2(int argc, char **argv, char **envp) {
    (void) envp;

    size_t iter = (size_t) atoi(argc > 1 ? argv[1] : DEFAULT_ITER);

    for (size_t i = 0; i < iter; i++)
    {
        sem_acquire(mutex_r2);
        sem_acquire(mutex_r1); //< DEADLOCK!

        // Access shared resources.
        uint32_t tmp = r2;
        r2 = r1;
        r1 = tmp;

        r2++;

        printf("[T2] { r1: %4i, r2: %4i }\n", r1, r2);

        sem_release(mutex_r1);
        sem_release(mutex_r2);
    }

    return 0;
}

void cmd_deadlock(int argc, char **argv)
{
    mutex_r1 = sem_create();
    mutex_r2 = sem_create();

    sem_init(mutex_r1);
    sem_init(mutex_r2);

    pid_t cpid1;

    char *iter_str = DEFAULT_ITER;
    if (argc > 2 && (strcmp(argv[1], "-i") == 0))
    {
        iter_str = argv[2];
    }

    if ((cpid1 = vfork()) == 0)
    {
        char *_argv[] = {"_deadlock_task1", iter_str, (char *) NULL};
        char *_envp[] = {(char *) NULL};

        execve((const char *) _deadlock_task1, _argv, _envp);

        printf("cmd_deadlock should not arrive here\n");
        return;
    }

    int status;
    waitpid(cpid1, &status, 0);

    sem_destroy(mutex_r1);
    sem_destroy(mutex_r2);
}