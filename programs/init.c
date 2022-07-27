/// @file init.c
/// @brief `init` program.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[], char *envp[])
{
    char *_argv[] = { "login", NULL };

    if (fork() == 0) {
        execv("/bin/login", _argv);
        printf("This is bad, I should not be here! EXEC NOT WORKING\n");
    }
    int status;
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while (1) {
        wait(&status);
    }
#pragma clang diagnostic pop
    return 0;
}
