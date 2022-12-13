/// @file t_stopcont.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "stdlib.h"
#include <sys/wait.h>

int child_pid;

void wait_for_child(int signr)
{
    printf("Signal received: %s\n", strsignal(signr));
    sleep(10);

    printf("Sending continue sig to child\n");
    if (kill(child_pid, SIGCONT) == -1)
        printf("Error sending signal\n");
}

int main(int argc, char *argv[])
{
    sigaction_t action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = wait_for_child;
    if (sigaction(SIGCHLD, &action, NULL) == -1) {
        printf("Failed to set signal handler. %d\n", SIGCHLD);
        return 1;
    }

    child_pid = fork();
    if (child_pid != 0) {
        printf("Child PID: %d\n", child_pid);

        sleep(2);
        printf("Sending stop sig to child\n");

#if 1
        if (kill(child_pid, SIGSTOP) == -1)
            printf("Errore invio stop\n");
#endif

        wait(NULL);
    } else {
        int c = 0;
        for (;;) {
            printf("c: %d\n", c++);
            sleep(3);
        }
    }

    return 0;
}
