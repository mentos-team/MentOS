/// @file shadow.c
/// @brief Functions for handling the shadow password file (`/etc/shadow`).
/// @copyright (c) 2005-2020 Rich Felker, et al.
/// This file is based on the code from libmusl.

#include "shadow.h"
#include "fcntl.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "unistd.h"
#include "sys/stat.h"
#include "ctype.h"
#include "limits.h"

/// @brief Converts a string to a long integer.
///
/// @details Parses a string into a long integer and advances the string pointer.
/// If the string starts with a colon or newline, returns -1.
///
/// @param s Pointer to the string to convert.
/// @return The parsed long integer.
static long xatol(char **s)
{
    long x;
    if (**s == ':' || **s == '\n') return -1;
    for (x = 0; **s - '0' < 10U; ++*s) x = 10 * x + (**s - '0');
    return x;
}

/// @brief Parses a shadow password entry from a string.
///
/// @details This function parses a line from the shadow password file into a `spwd` structure.
/// The fields in the shadow password file are separated by colons.
///
/// @param s The string containing the shadow password entry.
/// @param sp Pointer to the `spwd` structure where the parsed data will be stored.
/// @return 0 on success, -1 on failure.
int __parsespent(char *s, struct spwd *sp)
{
    sp->sp_namp = s;
    if (!(s = strchr(s, ':'))) return -1;
    *s = 0;

    sp->sp_pwdp = ++s;
    if (!(s = strchr(s, ':'))) return -1;
    *s = 0;

    s++;
    sp->sp_lstchg = xatol(&s);
    if (*s != ':') return -1;

    s++;
    sp->sp_min = xatol(&s);
    if (*s != ':') return -1;

    s++;
    sp->sp_max = xatol(&s);
    if (*s != ':') return -1;

    s++;
    sp->sp_warn = xatol(&s);
    if (*s != ':') return -1;

    s++;
    sp->sp_inact = xatol(&s);
    if (*s != ':') return -1;

    s++;
    sp->sp_expire = xatol(&s);
    if (*s != ':') return -1;

    s++;
    sp->sp_flag = xatol(&s);
    if (*s != '\n') return -1;
    return 0;
}

struct spwd *getspnam(const char *name)
{
    static struct spwd spwd_buf;
    struct spwd *result;
    char buffer[BUFSIZ];
    int e;
    int orig_errno = errno;

    // Call the reentrant function to get the shadow password entry.
    e = getspnam_r(name, &spwd_buf, buffer, BUFSIZ, &result);

    // Propagate error from getspnam_r if it fails.
    errno = e ? e : orig_errno;

    return result;
}

int getspnam_r(const char *name, struct spwd *spwd_buf, char *buf, size_t buflen, struct spwd **result)
{
    int rv = 0;                    // Return value to track errors (e.g., ERANGE).
    int fd;                        // File descriptor for the shadow file.
    size_t k;                      // Length of the current line read from the file.
    size_t l       = strlen(name); // Length of the username to search for.
    int skip       = 0;            // Flag to indicate whether the current line should be skipped.
    int orig_errno = errno;        // Preserve the original errno value for later restoration.

    if (!spwd_buf) {
        fprintf(stderr, "spwd_buf is NULL in getspnam_r.\n");
        return errno = EINVAL;
    }
    if (!result) {
        fprintf(stderr, "result is NULL in getspnam_r.\n");
        return errno = EINVAL;
    }

    // Initialize the result to NULL, indicating no match found yet.
    *result = 0;

    // Validate the user name for security:
    // - Disallow names starting with '.' or containing '/' (to prevent path traversal attacks).
    // - Disallow empty names.
    if (*name == '.' || strchr(name, '/') || !l) {
        return errno = EINVAL;
    }

    // Ensure the buffer is large enough to hold the username and additional data.
    if (buflen < l + 100) {
        return errno = ERANGE;
    }

    // Open the shadow file for reading.
    fd = open(SHADOW, O_RDONLY, 0);
    if (fd < 0) {
        return errno;
    }

    // Read lines from the shadow file.
    while (fgets(buf, buflen, fd) && (k = strlen(buf)) > 0) {
        // If skipping the line (due to being too long), or if the line does not match the user name:
        if (skip || strncmp(name, buf, l) || buf[l] != ':') {
            // Set skip if the line does not end with a newline.
            skip = buf[k - 1] != '\n';
            continue;
        }

        // If the line does not end with a newline, the buffer is too small.
        if (buf[k - 1] != '\n') {
            // Buffer overflow risk; set return value to ERANGE.
            rv = ERANGE;
            break;
        }

        // Parse the shadow entry from the line.
        // If parsing fails, continue to the next line.
        if (__parsespent(buf, spwd_buf) < 0) {
            continue;
        }

        // Set the result to the parsed shadow entry.
        *result = spwd_buf;
        // Exit the loop after finding the match.
        break;
    }

    // Close the file descriptor.
    close(fd);
    // Restore errno to its original value unless an error occurred.
    errno = rv ? rv : orig_errno;
    // Return 0 on success or an error code.
    return rv;
}
