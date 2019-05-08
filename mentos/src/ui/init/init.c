///                MentOS, The Mentoring Operating system project
/// @file init.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "init.h"
#include "wait.h"
#include "shell.h"
#include "errno.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include <misc/debug.h>

int main_init()
{
    pid_t cpid = vfork();

    if (cpid == 0)
    {
        char *_argv[] = {"shell", "hello", (char *) NULL};
        char *_envp[] = {"/", (char *) NULL};

        execve((const char *) shell, _argv, _envp);

        printf("This is bad, I should not be here! EXEC NOT WORKING\n");

        return 1;
    }

    int status;
    while (true)
    {
        if ((cpid = wait(&status)) > 0)
            dbg_print("Init has removed zombie children %d.\n", cpid);
    }

    return 0;
}
