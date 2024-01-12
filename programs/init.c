/// @file init.c
/// @brief `init` program.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[], char *envp[])
{
    char *_argv[] = { "login", NULL };
    int status;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while (1) {
        pid_t login = fork();
        if (login == 0) {
            execv("/bin/login", _argv);
            printf("This is bad, I should not be here! EXEC NOT WORKING\n");
        }

        while (wait(&status) != login);
    }
#pragma clang diagnostic pop
    return 0;
}
