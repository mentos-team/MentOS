/// @file t_exec.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/wait.h>
#include <string.h>

static inline void __print_usage(int argc, char *argv[])
{
    if (argc > 0) {
        printf("%s: Usage: %s <exec_type>\n", argv[0], argv[0]);
        printf("exec_type: execl, execlp, execle, execlpe, execv, execvp, execve, execvpe\n");
    }
}

int main(int argc, char *argv[])
{
    int status;
    if (argc != 2) {
        __print_usage(argc, argv);
        return 1;
    }

    if (setenv("ENV_VAR", "pwd0", 0) == -1) {
        printf("Failed to set env: `ENV_VAR`\n");
        return 1;
    }

    char *file        = "echo";
    char *path        = "/bin/echo";
    char *exec_argv[] = { "echo", "ENV_VAR: ${ENV_VAR}", NULL };
    char *exec_envp[] = { "PATH=/bin", "ENV_VAR=pwd1", NULL };

    if (fork() == 0) {
        if (strcmp(argv[1], "execl") == 0) {
            execl(path, "echo", "ENV_VAR: ${ENV_VAR}", NULL);
        } else if (strcmp(argv[1], "execlp") == 0) {
            execlp(file, "echo", "ENV_VAR: ${ENV_VAR}", NULL);
        } else if (strcmp(argv[1], "execle") == 0) {
            execle(path, "echo", "ENV_VAR: ${ENV_VAR}", exec_envp, NULL);
        } else if (strcmp(argv[1], "execlpe") == 0) {
            execlpe(file, "echo", "ENV_VAR: ${ENV_VAR}", exec_envp, NULL);
        } else if (strcmp(argv[1], "execv") == 0) {
            execv(path, exec_argv);
        } else if (strcmp(argv[1], "execvp") == 0) {
            execvp(file, exec_argv);
        } else if (strcmp(argv[1], "execve") == 0) {
            execve(path, exec_argv, exec_envp);
        } else if (strcmp(argv[1], "execvpe") == 0) {
            execvpe(file, exec_argv, exec_envp);
        } else {
            __print_usage(argc, argv);
            return 1;
        }
        printf("Exec failed.\n");
        return 1;
    }
    wait(&status);
    return 0;
}
