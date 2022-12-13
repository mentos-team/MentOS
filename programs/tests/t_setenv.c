/// @file t_setenv.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    char *_argv[] = { "/bin/tests/t_getenv", NULL };
    int status;

    if (setenv("ENV_VAR", "pwd0", 0) == -1) {
        printf("Failed to set env: `PWD`\n");
        return 1;
    }

    if (fork() == 0) {
        execv(_argv[0], _argv);
        printf("This is bad, I should not be here! EXEC NOT WORKING\n");
    }

    wait(&status);
    return 0;
}
