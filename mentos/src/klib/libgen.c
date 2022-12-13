/// @file libgen.c
/// @brief String routines.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/syscall.h"
#include "libgen.h"
#include "string.h"
#include "limits.h"
#include "assert.h"
#include "mem/paging.h"

int parse_path(char *out, char **cur, char sep, size_t max)
{
    if (**cur == '\0') {
        return 0;
    }

    *out++ = **cur;
    ++*cur;
    --max;

    while ((max > 0) && (**cur != '\0') && (**cur != sep)) {
        *out++ = **cur;
        ++*cur;
        --max;
    }

    *out = '\0';

    return 1;
}

char *dirname(const char *path)
{
    static char s[PATH_MAX];

    static char dot[2] = ".";

    // Check the input path.
    if (path == NULL) {
        return dot;
    }

    // Copy the path to the support string.
    strcpy(s, path);

    // Get the last occurrence of '/'.
    char *last_slash = strrchr(s, '/');

    if (last_slash == s) {
        // If the slash is acutally the first character of the string, move the
        // pointer to the last slash after it.
        ++last_slash;
    } else if ((last_slash != NULL) && (last_slash[1] == '\0')) {
        // If the slash is the last character, we need to search before it.
        last_slash = memchr(s, '/', last_slash - s);
    }

    if (last_slash != NULL) {
        // If we have found it, close the string.
        last_slash[0] = '\0';
    } else {
        // Otherwise, return '.'.
        return dot;
    }

    return s;
}

char *basename(const char *path)
{
    char *p = strrchr(path, '/');

    return p ? p + 1 : (char *)path;
}

char *realpath(const char *path, char *resolved)
{
    assert(path && "Provided null path.");
    if (resolved == NULL)
        resolved = kmalloc(sizeof(char) * PATH_MAX);
    char abspath[PATH_MAX];
    // Initialize the absolute path.
    memset(abspath, '\0', PATH_MAX);
    int remaining;
    if (path[0] != '/') {
        // Get the working directory of the current task.
        sys_getcwd(abspath, PATH_MAX);
        // Check the absolute path.
        assert((strlen(abspath) > 0) && "Current working directory is set.");
        // Check that the current working directory is an absolute path.
        assert((abspath[0] == '/') && "Current working directory is not an absolute path.");
        // Count the remaining space in the absolute path.
        remaining = PATH_MAX - strlen(abspath) - 1;
        // Add the separator to the end (se strncat for safety).
        strncat(abspath, "/", remaining);
        // Set the number of characters that should be copied,
        // based on the current absolute path.
        remaining = PATH_MAX - strlen(abspath) - 1;
        // Append the path.
        strncat(abspath, path, remaining);
    } else {
        // Copy the path into the absolute path.
        strncpy(abspath, path, PATH_MAX - 1);
    }
    // Count the remaining space in the absolute path.
    remaining = PATH_MAX - strlen(abspath) - 1;
    // Add the separator to the end (se strncat for safety).
    strncat(abspath, "/", remaining);

    int absidx = 0, pathidx = 0;

    while (abspath[absidx]) {
        // Skip multiple consecutive / characters
        if (!strncmp("//", abspath + absidx, 2)) {
            absidx++;
        }
        // Go to previous directory if /../ is found
        else if (!strncmp("/../", abspath + absidx, 4)) {
            // Go to a valid path character (pathidx points to the next one)
            if (pathidx)
                pathidx--;
            while (pathidx && resolved[pathidx] != '/') {
                pathidx--;
            }
            absidx += 3;
        } else if (!strncmp("/./", abspath + absidx, 3)) {
            absidx += 2;
        } else {
            resolved[pathidx++] = abspath[absidx++];
        }
    }
    // Remove the last /
    if (pathidx > 1)
        resolved[pathidx - 1] = '\0';
    else
        resolved[1] = '\0';
    return resolved;
}
