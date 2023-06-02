/// @file t_schedfb.c
/// @brief Start the scheduler feedback session.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <sys/wait.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    pid_t cpid;
    printf("First test, the child will sleep, thus, they will not be scheduled.\n");
    for (int i = 0; i < 10; ++i) {
        if ((cpid = fork()) == 0) {
            execl("/bin/tests/t_alarm", "t_alarm", NULL);
            return 0;
        }
    }
    while (wait(NULL) != -1) continue;
    return 0;
}
