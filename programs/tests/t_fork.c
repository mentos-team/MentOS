/// @file t_fork.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        return 1;
    }
    char *ptr;
    int N;
    N = strtol(argv[1], &ptr, 10);
    pid_t cpid, mypid;
#if 0
    char buffer[256];

    N = strtol(argv[1], &ptr, 10);

    char *_argv[] = {
        "/bin/tests/t_fork",
        itoa(buffer, N - 1, 10),
        NULL
    };

    printf("N = %d\n", N);
    if (N > 1) {

        printf("Keep caling (N = %d)\n", N);

        if ((cpid = fork()) == 0) {
            
            printf("I'm the child (pid = %d, N = %d)!\n", getpid(), N);

            execv(_argv[0], _argv);
            
        }
        printf("Will wait for %d\n", N, cpid);
        while (wait(NULL) != -1) continue;
    }
#else
    while (1) {
        mypid = getpid();
        if (N > 0) {
            if ((cpid = fork()) == 0) {
                N -= 1;
                continue;
            }
            printf("I'm %d and I will wait for %d (N = %d)!\n", mypid, cpid, N);
            while (wait(NULL) != -1) continue;
            printf("I'm %d and I waited for %d (N = %d)!\n", mypid, cpid, N);
        } else {
            printf("I'm %d and I will not wait!\n", mypid);
        }
        break;
    }
#endif
    return 0;
}
