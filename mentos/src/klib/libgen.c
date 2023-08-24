/// @file libgen.c
/// @brief String routines.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "system/syscall.h"
#include "assert.h"
#include "io/debug.h"
#include "libgen.h"
#include "limits.h"
#include "mem/paging.h"
#include "string.h"

int dirname(const char *path, char *buffer, size_t buflen)
{
    if ((path == NULL) || (buffer == NULL) || (buflen == 0)) {
        return 0;
    }
    // Search for the last slash.
    const char *last_slash = NULL;
    for (const char *it = path; *it; it++) {
        if ((*it) == '/') {
            last_slash = it;
        }
    }
    // If we were able to find a slash, and the slash is not in the first
    // position, copy the substring.
    if (last_slash) {
        // Get the length of the substring, if the last slash is at the beginning,
        // add 1.
        size_t dirlen = last_slash - path + (last_slash == path);
        // Check if the path will fit inside the buffer.
        if (dirlen >= buflen) {
            return 0;
        }
        // Copy the substring.
        strncpy(buffer, path, dirlen);
        // Close the buffer.
        buffer[dirlen] = 0;
    } else {
        strcpy(buffer, ".");
    }
    return 1;
}

const char *basename(const char *path)
{
    // Search for the last slash.
    const char *last_slash = NULL;
    for (const char *it = path; *it; it++) {
        if ((*it) == '/') {
            last_slash = it;
        }
    }
    return last_slash ? last_slash + 1 : path;
}

char *realpath(const char *path, char *buffer, size_t buflen)
{
    assert(path && "Provided null path.");
    assert(buffer && "Provided null buffer.");
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
            if (pathidx) {
                pathidx--;
            }
            while (pathidx && buffer[pathidx] != '/') {
                pathidx--;
            }
            absidx += 3;
        } else if (!strncmp("/./", abspath + absidx, 3)) {
            absidx += 2;
        } else {
            buffer[pathidx++] = abspath[absidx++];
        }
    }
    // Remove the last /
    if (pathidx > 1) {
        buffer[pathidx - 1] = '\0';
    } else {
        buffer[1] = '\0';
    }
    return buffer;
}
