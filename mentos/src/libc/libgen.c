///                MentOS, The Mentoring Operating system project
/// @file libgen.c
/// @brief String routines.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "libgen.h"
#include "string.h"
#include "initrd.h"

int parse_path(char *out, char **cur, char sep, size_t max)
{
    if (**cur == '\0')
    {
        return 0;
    }

    *out++ = **cur;
    ++*cur;
    --max;

    while ((max > 0) && (**cur != '\0') && (**cur != sep))
    {
        *out++ = **cur;
        ++*cur;
        --max;
    }

    *out = '\0';

    return 1;
}

char *dirname(const char *path)
{
    static char s[MAX_PATH_LENGTH];

    static char dot[2] = ".";

    // Check the input path.
    if (path == NULL) return dot;

    // Copy the path to the support string.
    strcpy(s, path);

    // Get the last occurrence of '/'.
    char *last_slash = strrchr(s, '/');

    if (last_slash == s)
    {
        // If the slash is acutally the first character of the string, move the
        // pointer to the last slash after it.
        ++last_slash;
    }
    else if ((last_slash != NULL) && (last_slash[1] == '\0'))
    {
        // If the slash is the last character, we need to search before it.
        last_slash = memchr(s, '/', last_slash - s);
    }

    if (last_slash != NULL)
    {
        // If we have found it, close the string.
        last_slash[0] = '\0';
    }
    else
    {
        // Otherwise, return '.'.
        return dot;
    }

    return s;
}

char *basename(const char *path)
{
    char *p = strrchr(path, '/');

    return p ? p + 1 : (char *) path;
}
