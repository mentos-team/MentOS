/// @file setenv.c
/// @brief Defines the functions used to manipulate the environmental variables.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <assert.h>
#include "sys/errno.h"
#include "string.h"
#include "stdlib.h"

char **environ;

static char **__environ      = NULL;
static size_t __environ_size = 0;

/// @brief Finds the entry in the environ.
/// @param name the name of the entry we are looking for.
/// @param name_len the length of the name we received.
/// @return the index of the entry, or -1 if we did not find it.
static inline int __find_entry(const char *name, const size_t name_len)
{
    if (environ) {
        int index = 0;
        for (char **ptr = environ; *ptr; ++ptr, ++index)
            if (!strncmp((*ptr), name, name_len) && (*ptr)[name_len] == '=')
                return index;
    }
    return -1;
}

/// @brief Makes a clone of the current environ.
static void __clone_environ()
{
    if (environ) {
        // Count the number of variables.
        __environ_size = 0;
        for (char **ptr = environ; *ptr; ++ptr) { ++__environ_size; }
        // Allocate the space.
        __environ = malloc(sizeof(char *) * (__environ_size + 2));
        for (int i = 0; i < __environ_size; ++i) {
            size_t entry_len = strlen(environ[i]) + 1;
            __environ[i]     = malloc(sizeof(char) * entry_len);
            memcpy(__environ[i], environ[i], entry_len);
        }
        __environ[__environ_size]     = (char *)NULL;
        __environ[__environ_size + 1] = (char *)NULL;
        __environ_size += 2;
        // Set the new environ.
        environ = __environ;
    }
}

int setenv(const char *name, const char *value, int replace)
{
    // There must be always an environ variable set.
    assert(environ && "There is no `environ` set.");
    // Check the name.
    if (name == NULL || *name == '\0' || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }
    if (__environ == NULL) {
        __clone_environ();
        environ = __environ;
    }
    const size_t name_len  = strlen(name);
    const size_t value_len = strlen(value) + 1;
    const size_t total_len = name_len + value_len + 1;
    //LOCK;
    if (environ == NULL) {
        return -1;
    }
    // Find the entry.
    int index = __find_entry(name, name_len);
    if (index >= 0) {
        if (!replace) {
            //UNLOCK;
            return -1;
        }
    } else {
        // Get the size of environ;
        index = __environ_size - 2;
        // Extend the environ dynamically allocated memory.
        char **new_environ = (char **)realloc(environ, sizeof(char *) * (__environ_size + 1));
        // Check the pointer.
        if (new_environ == NULL) {
            //UNLOCK;
            return -1;
        }
        // Increment the size.
        __environ_size += 1;
        // Close the environment.
        new_environ[__environ_size - 2] = NULL;
        new_environ[__environ_size - 1] = NULL;
        // Update all the variables.
        environ = __environ = new_environ;
    }
    // Free the previous entry.
    if (environ[index])
        free(environ[index]);
    // Allocate the new entry.
    environ[index] = malloc(total_len);
    // Memcopy because we do not want the null terminating character.
    memcpy(environ[index], name, name_len);
    // Set the equal.
    environ[index][name_len] = '=';
    // Add the value.
    memcpy(&environ[index][name_len + 1], value, value_len);
    //UNLOCK;
    return 0;
}

int unsetenv(const char *name)
{
    if (name == NULL || *name == '\0' || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }
    if (__environ == NULL) {
        __clone_environ();
        environ = __environ;
    }
    size_t len = strlen(name);
    //LOCK;
    char **ep = environ;
    while (*ep != NULL) {
        if (!strncmp(*ep, name, len) && (*ep)[len] == '=') {
            /* Found it.  Remove this pointer by moving later ones back.  */
            char **dp = ep;
            do dp[0] = dp[1];
            while (*dp++);
            /* Continue the loop in case NAME appears again.  */
        } else {
            ++ep;
        }
    }
    //UNLOCK;
    return 0;
}

char *getenv(const char *name)
{
    size_t name_len = strlen(name);
    int index       = __find_entry(name, name_len);
    if (index < 0) {
        return NULL;
    }
    size_t env_len = strlen(environ[index]);
    if ((name_len + 1) < env_len) {
        return &environ[index][name_len + 1];
    }
    return NULL;
}
