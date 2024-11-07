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

/// Defines the buffer size for reading lines from the shadow file.
#define LINE_LIM 256

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
    static char *line;
    struct spwd *result;
    int e;
    int orig_errno = errno;

    if (!line) line = malloc(LINE_LIM);
    if (!line) return 0;
    e     = getspnam_r(name, &spwd_buf, line, LINE_LIM, &result);
    errno = e ? e : orig_errno;
    return result;
}

int getspnam_r(const char *name, struct spwd *spwd_buf, char *buf, size_t buflen, struct spwd **result)
{
    char path[20 + NAME_MAX];
    int rv = 0;
    int fd;
    size_t k, l = strlen(name);
    int skip = 0;
    int cs;
    int orig_errno = errno;

    *result = 0;

    /* Disallow potentially-malicious user names */
    if (*name == '.' || strchr(name, '/') || !l)
        return errno = EINVAL;

    /* Buffer size must at least be able to hold name, plus some.. */
    if (buflen < l + 100)
        return errno = ERANGE;

    fd = open(SHADOW, O_RDONLY, 0);
    if (fd < 0) {
        return errno;
    }

    while (fgets(buf, buflen, fd) && (k = strlen(buf)) > 0) {
        if (skip || strncmp(name, buf, l) || buf[l] != ':') {
            skip = buf[k - 1] != '\n';
            continue;
        }
        if (buf[k - 1] != '\n') {
            rv = ERANGE;
            break;
        }

        if (__parsespent(buf, spwd_buf) < 0) continue;
        *result = spwd_buf;
        break;
    }
    errno = rv ? rv : orig_errno;
    return rv;
}
