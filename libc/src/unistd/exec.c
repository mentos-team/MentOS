/// @file exec.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "fcntl.h"
#include "limits.h"
#include "stdarg.h"
#include "stdlib.h"
#include "string.h"
#include "sys/stat.h"
#include "system/syscall_types.h"
#include "unistd.h"

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
    char *path  = strdup(PATH_VAR);
    // Iterate through the path entries.
    char *token = strtok(path, ":");
    while (token != NULL) {
        strcpy(buf, token);
        strcat(buf, "/");
        strcat(buf, file);
        if (stat(buf, &stat_buf) == 0) {
            if (stat_buf.st_mode & S_IXUSR) {
                // TODO(enrico): Check why `init` has problems with this free.
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

int execve(const char *path, char *const argv[], char *const envp[])
{
    long __res;
    __inline_syscall_3(__res, execve, path, argv, envp);
    __syscall_return(int, __res);
}

int execv(const char *path, char *const argv[]) { return execve(path, argv, environ); }

int execvp(const char *file, char *const argv[]) { return execvpe(file, argv, environ); }

int execvpe(const char *file, char *const argv[], char *const envp[])
{
    if (!file || !argv) {
        errno = ENOENT;
        return -1;
    }

    // Default environment if envp/environ are NULL
    static char *default_env[] = {
        "PATH=/bin:/usr/bin",
        "HOME=/",
        NULL
    };

    // Pointer to the actual environment we will be using
    char *const *use_envp = envp ? envp : (environ ? environ : default_env);

    // If the name already contains '/', we don't use PATH: we call execve directly
    if (strchr(file, '/')) {
        return execve(file, argv, (char *const *)use_envp);
    }

    // Look for PATH=... inside use_envp
    const char *path = NULL;
    for (char *const *e = use_envp; e && *e; ++e) {
        if (strncmp(*e, "PATH=", 5) == 0) {
            path = *e + 5;  // Skips "PATH="
            break;
        }
    }

    // If PATH is not present in the environment, it uses a sensible default value
    if (path == NULL || path[0] == '\0') {
        path = "/bin:/usr/bin";
    }

    char absolute_path[PATH_MAX];

    // Iterates on the elements of PATH with the ':' separator
    const char *p = path;
    while (*p != '\0') {
        // Start of the segment
        const char *start = p;

        // Finds the end of the segment or the end of the string
        while (*p != '\0' && *p != ':') {
            p++;
        }
        size_t len = (size_t)(p - start);

        if (len > 0) {
            // Constructs "segment/file" in absolute_path
            if (len + 1 + strlen(file) + 1 < sizeof(absolute_path)) {
                memcpy(absolute_path, start, len);
                absolute_path[len] = '/';
                strcpy(absolute_path + len + 1, file);

                execve(absolute_path, argv, (char *const *)use_envp);

                // If execve returns, it failed: if not ENOENT, we stop with error
                if (errno != ENOENT) {
                    return -1;
                }
            }
        }

        // If we are at ':', we skip to the next segment
        if (*p == ':') {
            p++;
        }
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
    for (int i = 1; i <= argc; ++i) {
        argv[i] = va_arg(ap, char *);
    }
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
    for (int i = 1; i < argc; ++i) {
        argv[i] = va_arg(ap, char *);
    }
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
    for (int i = 1; i < argc; ++i) {
        argv[i] = va_arg(ap, char *);
    }
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
    for (int i = 1; i < argc; ++i) {
        argv[i] = va_arg(ap, char *);
    }
    // Close argv.
    argv[argc] = NULL;

    // Phase 3: Store the pointer to the environ.
    char **envp = va_arg(ap, char **);
    va_end(ap);

    // Now, we can call `execve` plain and simple.
    return execvpe(file, argv, envp);
}
