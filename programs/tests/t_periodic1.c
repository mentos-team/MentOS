/// @file t_periodic1.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <sched.h>
#include <sys/unistd.h>
#include <strerror.h>

int main(int argc, char *argv[])
{
    pid_t cpid = getpid();
    sched_param_t param;
    // Get current parameters.
    sched_getparam(cpid, &param);
    // Change parameters.
    param.period      = 5000;
    param.deadline    = 5000;
    param.is_periodic = true;
    // Set modified parameters.
    sched_setparam(cpid, &param);
    int counter   = 0;
    if (fork() == 0) {
        char *_argv[] = { "/bin/periodic2", NULL };
        execv(_argv[0], _argv);
        printf("This is bad, I should not be here! EXEC NOT WORKING\n");
    }
    if (fork() == 0) {
        char *_argv[] = { "/bin/periodic3", NULL };
        execv(_argv[0], _argv);
        printf("This is bad, I should not be here! EXEC NOT WORKING\n");
    }

    while (1) {
        if (++counter == 10) {
            counter = 0;
        }
        printf("[periodic1]counter %d\n",counter);
        if (waitperiod() == -1) {
            printf("[%s] Error in waitperiod: %s\n", argv[0], strerror(errno));
            break;
        }
    }
    return 0;
}
