/// @file exec.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <debug.h>
#include "sys/unistd.h"
#include "system/syscall_types.h"
#include "sys/errno.h"

#include "sys/stat.h"
#include "stdlib.h"
#include "string.h"
#include "stdarg.h"
#include "fcntl.h"

extern char **environ;

/// @brief Default `PATH`.
#define DEFAULT_PATH "/bin:/usr/bin"

/// @brief Finds an executable inside the PATH entries.
/// @param file    The file to search.
/// @param buf     The buffer where we will store the absolute path.
/// @param buf_len The length of the buffer.
/// @return 0  if we have found the file inside the entries of PATH,
///         -1 otherwise.
static inline int __find_in_path(const char *file, char *buf, size_t buf_len)
{
    // Determine the search path.
    char *PATH_VAR = getenv("PATH");
    if (PATH_VAR == NULL) {
        PATH_VAR = DEFAULT_PATH;
    }
    // Prepare a stat object for later.
    stat_t stat_buf;
    // Copy the path.
    char *path = strdup(PATH_VAR);
    // Iterate through the path entries.
    char *token = strtok(path, ":");
    while (token != NULL) {
        strcpy(buf, token);
        strcat(buf, "/");
        strcat(buf, file);
        if (stat(buf, &stat_buf) == 0) {
            if (stat_buf.st_mode & S_IXUSR) {
                // TODO: Check why `init` has problems with this free.
                //       To reproduce the problem use this in init.c:
                //           execvp("login", _argv);
                free(path);
                return 0;
            }
        }
        token = strtok(NULL, ":");
    }
    free(path);
    // We did not find the file inside PATH.
    errno = ENOENT;
    return -1;
}

/// @brief Replaces the current process image with a new process
///        image (argument vector), allows the caller to specify
///        the environment of the executed program via `envp`.
/// @param path The absolute path to the binary file to execute.
/// @param argv A vector of one or more pointers to null-terminated strings that represent
///             the argument list available to the executed program.
/// @param envp A vector of one or more pointers to null-terminated strings that represent
///             the environment list available to the executed program.
/// @return Returns -1 only if an error has occurred, and sets errno.
_syscall3(int, execve, const char *, path, char *const *, argv, char *const *, envp)

int execv(const char *path, char *const argv[])
{
    return execve(path, argv, environ);
}

int execvp(const char *file, char *const argv[])
{
    if (!file || !argv || !environ) {
        errno = ENOENT;
        return -1;
    }
    if (file[0] == '/') {
        return execve(file, argv, environ);
    }
    // Prepare a buffer for the absolute path.
    char absolute_path[PATH_MAX] = { 0 };
    // Find the file inside the entries of the PATH variable.
    if (__find_in_path(file, absolute_path, PATH_MAX) == 0) {
        return execve(absolute_path, argv, environ);
    }
    errno = ENOENT;
    return -1;
}

int execvpe(const char *file, char *const argv[], char *const envp[])
{
    if (!file || !argv || !envp) {
        errno = ENOENT;
        return -1;
    }
    if (file[0] == '/') {
        return execve(file, argv, envp);
    }
    // Prepare a buffer for the absolute path.
    char absolute_path[PATH_MAX];
    // Find the file inside the entries of the PATH variable.
    if (__find_in_path(file, absolute_path, PATH_MAX) == 0) {
        return execve(absolute_path, argv, envp);
    }
    errno = ENOENT;
    return -1;
}

int execl(const char *path, const char *arg, ...)
{
    va_list ap;
    int argc;

    // Phase 1: Count the arguments.
    va_start(ap, arg);
    for (argc = 1; va_arg(ap, const char *); ++argc) {
        if (argc == INT_MAX) {
            va_end(ap);
            errno = E2BIG;
            return -1;
        }
    }
    va_end(ap);

    // Phase 2: Store the arguments inside a vector.
    char *argv[argc + 1];
    va_start(ap, arg);
    argv[0] = (char *)arg;
    for (int i = 1; i <= argc; ++i)
        argv[i] = va_arg(ap, char *);
    va_end(ap);

    // Now, we can call `execve` plain and simple.
    return execve(path, argv, environ);
}

int execlp(const char *file, const char *arg, ...)
{
    va_list ap;
    int argc;

    // Phase 1: Count the arguments.
    va_start(ap, arg);
    for (argc = 1; va_arg(ap, const char *); ++argc) {
        if (argc == INT_MAX) {
            va_end(ap);
            errno = E2BIG;
            return -1;
        }
    }
    va_end(ap);

    // Phase 2: Store the arguments inside a vector.
    char *argv[argc + 1];
    va_start(ap, arg);
    argv[0] = (char *)arg;
    for (int i = 1; i < argc; ++i)
        argv[i] = va_arg(ap, char *);
    va_end(ap);

    // Close argv.
    argv[argc] = NULL;

    // Now, we can call `execve` plain and simple.
    return execvpe(file, argv, environ);
}

int execle(const char *path, const char *arg, ...)
{
    va_list ap;
    int argc;

    // Phase 1: Count the arguments.
    va_start(ap, arg);
    for (argc = 1; va_arg(ap, const char *); ++argc) {
        if (argc == INT_MAX) {
            va_end(ap);
            errno = E2BIG;
            return -1;
        }
    }
    va_end(ap);

    argc -= 1;
    if (argc < 0) {
        errno = EINVAL;
        return -1;
    }

    // Phase 2: Store the arguments inside a vector.
    char *argv[argc + 1];
    va_start(ap, arg);
    argv[0] = (char *)arg;
    for (int i = 1; i < argc; ++i)
        argv[i] = va_arg(ap, char *);
    // Close argv.
    argv[argc] = NULL;

    // Phase 3: Store the pointer to the environ.
    char **envp = va_arg(ap, char **);
    va_end(ap);

    // Now, we can call `execve` plain and simple.
    return execve(path, argv, envp);
}

int execlpe(const char *file, const char *arg, ...)
{
    va_list ap;
    int argc;

    // Phase 1: Count the arguments.
    va_start(ap, arg);
    for (argc = 1; va_arg(ap, const char *); ++argc) {
        if (argc == INT_MAX) {
            va_end(ap);
            errno = E2BIG;
            return -1;
        }
    }
    va_end(ap);

    // We don't need to count `envp` among the arguments.
    if ((argc -= 1) < 0) {
        errno = EINVAL;
        return -1;
    }

    // Phase 2: Store the arguments inside a vector.
    char *argv[argc + 1];
    va_start(ap, arg);
    argv[0] = (char *)arg;
    for (int i = 1; i < argc; ++i)
        argv[i] = va_arg(ap, char *);
    // Close argv.
    argv[argc] = NULL;

    // Phase 3: Store the pointer to the environ.
    char **envp = va_arg(ap, char **);
    va_end(ap);

    // Now, we can call `execve` plain and simple.
    return execvpe(file, argv, envp);
}
