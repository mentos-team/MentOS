/// @file libgen.c
/// @brief String routines.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
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
