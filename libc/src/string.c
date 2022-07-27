/// @file string.c
/// @brief String routines.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include "string.h"
#include "ctype.h"
#include "stdio.h"
#include "stdlib.h"

char *strncpy(char *destination, const char *source, size_t num)
{
    char *start = destination;
    while (num && (*destination++ = *source++)) {
        num--;
    }
    if (num) {
        while (--num) {
            *destination++ = '\0';
        }
    }

    return start;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    if (!n)
        return 0;
    while ((--n > 0) && (*s1) && (*s2) && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int stricmp(const char *s1, const char *s2)
{
    while (*s2 != 0 && toupper(*s1) == toupper(*s2)) {
        s1++, s2++;
    }

    return (toupper(*s1) - toupper(*s2));
}

int strnicmp(const char *s1, const char *s2, size_t n)
{
    int f, l;

    do {
        if (((f = (unsigned char)(*(s1++))) >= 'A') && (f <= 'Z')) {
            f -= 'A' - 'a';
        }
        if (((l = (unsigned char)(*(s2++))) >= 'A') && (l <= 'Z')) {
            l -= 'A' - 'a';
        }
    } while (--n && f && (f == l));

    return f - l;
}

char *strchr(const char *s, int ch)
{
    while (*s && *s != (char)ch) {
        s++;
    }
    if (*s == (char)ch)
        return (char *)s;
    {
        return NULL;
    }
}

char *strrchr(const char *s, int ch)
{
    char *start = (char *)s;

    while (*s++) {}

    while (--s != start && *s != (char)ch) {}

    if (*s == (char)ch) {
        return (char *)s;
    }

    return NULL;
}

char *strstr(const char *str1, const char *str2)
{
    char *cp = (char *)str1;
    char *s1, *s2;

    if (!*str2) {
        return (char *)str1;
    }

    while (*cp) {
        s1 = cp;
        s2 = (char *)str2;

        while (*s1 && *s2 && !(*s1 - *s2)) {
            s1++, s2++;
        }
        if (!*s2) {
            return cp;
        }
        cp++;
    }

    return NULL;
}

size_t strspn(const char *string, const char *control)
{
    const char *str  = string;
    const char *ctrl = control;

    char map[32];
    size_t n;

    // Clear out bit map.
    for (n = 0; n < 32; n++) {
        map[n] = 0;
    }

    // Set bits in control map.
    while (*ctrl) {
        map[*ctrl >> 3] |= (char)(1 << (*ctrl & 7));
        ctrl++;
    }

    // 1st char NOT in control map stops search.
    if (*str) {
        n = 0;
        while (map[*str >> 3] & (1 << (*str & 7))) {
            n++;
            str++;
        }

        return n;
    }

    return 0;
}

size_t strcspn(const char *string, const char *control)
{
    const char *str  = string;
    const char *ctrl = control;

    char map[32];
    size_t n;

    // Clear out bit map.
    for (n = 0; n < 32; n++)
        map[n] = 0;

    // Set bits in control map.
    while (*ctrl) {
        map[*ctrl >> 3] |= (char)(1 << (*ctrl & 7));
        ctrl++;
    }

    // 1st char in control map stops search.
    n = 0;
    map[0] |= 1;
    while (!(map[*str >> 3] & (1 << (*str & 7)))) {
        n++;
        str++;
    }

    return n;
}

char *strpbrk(const char *string, const char *control)
{
    const char *str  = string;
    const char *ctrl = control;

    char map[32];
    int n;

    // Clear out bit map.
    for (n = 0; n < 32; n++)
        map[n] = 0;

    // Set bits in control map.
    while (*ctrl) {
        map[*ctrl >> 3] |= (char)(1 << (*ctrl & 7));
        ctrl++;
    }

    // 1st char in control map stops search.
    while (*str) {
        if (map[*str >> 3] & (1 << (*str & 7))) {
            return (char *)str;
        }
        str++;
    }

    return NULL;
}

void *memmove(void *dst, const void *src, size_t n)
{
    void *ret = dst;

    if (dst <= src || (char *)dst >= ((char *)src + n)) {
        /* Non-overlapping buffers; copy from lower addresses to higher
         * addresses.
         */
        while (n--) {
            *(char *)dst = *(char *)src;
            dst          = (char *)dst + 1;
            src          = (char *)src + 1;
        }
    } else {
        // Overlapping buffers; copy from higher addresses to lower addresses.
        dst = (char *)dst + n - 1;
        src = (char *)src + n - 1;

        while (n--) {
            *(char *)dst = *(char *)src;
            dst          = (char *)dst - 1;
            src          = (char *)src - 1;
        }
    }

    return ret;
}

void *memchr(const void *ptr, int ch, size_t n)
{
    while (n && (*(unsigned char *)ptr != (unsigned char)ch)) {
        ptr = (unsigned char *)ptr + 1;
        n--;
    }

    return (n ? (void *)ptr : NULL);
}

char *strlwr(char *s)
{
    char *p = s;

    while (*p) {
        *p = (char)tolower(*p);
        p++;
    }

    return s;
}

char *strupr(char *s)
{
    char *p = s;

    while (*p) {
        *p = (char)toupper(*p);
        p++;
    }

    return s;
}

char *strncat(char *s1, const char *s2, size_t n)
{
    char *start = s1;

    while (*s1++) {}

    s1--;

    while (n--) {
        if (!(*s1++ = *s2++))
            return start;
    }

    *s1 = '\0';

    return start;
}

char *strnset(char *s, int c, size_t n)
{
    while (n-- && *s) {
        *s++ = (char)c;
    }

    return s;
}

char *strrev(char *s)
{
    char *start = s;
    char *left  = s;
    char ch;

    while (*s++) {}
    s -= 2;

    while (left < s) {
        ch      = *left;
        *left++ = *s;
        *s--    = ch;
    }

    return start;
}

char *strtok_r(char *str, const char *delim, char **saveptr)
{
    char *s;
    const char *ctrl = delim;

    char map[32];
    int n;

    // Clear delim map.
    for (n = 0; n < 32; n++) {
        map[n] = 0;
    }

    // Set bits in delimiter table.
    do {
        map[*ctrl >> 3] |= (char)(1 << (*ctrl & 7));
    } while (*ctrl++);

    /* Initialize s. If str is NULL, set s to the saved
     * pointer (i.e., continue breaking tokens out of the str
     * from the last strtok call).
     */
    if (str) {
        s = str;
    } else {
        s = *saveptr;
    }

    /* Find beginning of token (skip over leading delimiters). Note that
     * there is no token iff this loop sets s to point to the terminal
     * null (*s == '\0').
     */
    while ((map[*s >> 3] & (1 << (*s & 7))) && *s) {
        s++;
    }

    str = s;

    /* Find the end of the token. If it is not the end of the str,
     * put a null there.
     */
    for (; *s; s++) {
        if (map[*s >> 3] & (1 << (*s & 7))) {
            *s++ = '\0';

            break;
        }
    }

    // Update nexttoken.
    *saveptr = s;

    // Determine if a token has been found.
    if (str == (char *)s) {
        return NULL;
    } else {
        return str;
    }
}

// Intrinsic functions.

/*
 * #pragma function(memset)
 * #pragma function(memcmp)
 * #pragma function(memcpy)
 * #pragma function(strcpy)
 * #pragma function(strlen)
 * #pragma function(strcat)
 * #pragma function(strcmp)
 * #pragma function(strset)
 */

void *memset(void *ptr, int value, size_t num)
{
    // Truncate c to 8 bits.
    value = (value & 0xFF);

    char *dst = (char *)ptr;

    // Initialize the rest of the size.
    while (num--) {
        *dst++ = (char)value;
    }

    return ptr;
}

int memcmp(const void *ptr1, const void *ptr2, size_t n)
{
    if (!n) {
        return 0;
    }

    while (--n && *(char *)ptr1 == *(char *)ptr2) {
        ptr1 = (char *)ptr1 + 1;
        ptr2 = (char *)ptr2 + 1;
    }

    return *((unsigned char *)ptr1) - *((unsigned char *)ptr2);
}

void *memcpy(void *ptr1, const void *ptr2, size_t num)
{
    char *_dst       = ptr1;
    const char *_src = ptr2;

    while (num--) {
        *_dst++ = *_src++;
    }

    return ptr1;
}

void *memccpy(void *ptr1, const void *ptr2, int c, size_t n)
{
    while (n && (*((char *)(ptr1 = (char *)ptr1 + 1) - 1) =
                     *((char *)(ptr2 = (char *)ptr2 + 1) - 1)) != (char)c) {
        n--;
    }

    return n ? ptr1 : NULL;
}

char *strcpy(char *dst, const char *src)
{
    char *save = dst;

    while ((*dst++ = *src++) != '\0') {}

    return save;
}

size_t strlen(const char *s)
{
    const char *eos;

    for (eos = s; *eos != 0; ++eos) {}

    long len = eos - s;

    return (len < 0) ? 0 : (size_t)len;
}

size_t strnlen(const char *s, size_t count)
{
    const char *sc;

    for (sc = s; *sc != '\0' && count--; ++sc) {}

    long len = sc - s;

    return (len < 0) ? 0 : (size_t)len;
}

int strcmp(const char *s1, const char *s2)
{
    int ret         = 0;
    const char *s1t = s1, *s2t = s2;

    for (; !(ret = *s1t - *s2t) && *s2t; ++s1t, ++s2t) {}

    return (ret < 0) ? -1 : (ret > 0) ? 1 :
                                        0;
}

char *strcat(char *dst, const char *src)
{
    char *cp = dst;

    while (*cp) {
        cp++;
    }

    while ((*cp++ = *src++) != '\0') {}

    return dst;
}

char *strset(char *s, int c)
{
    char *start = s;

    while (*s) {
        *s++ = (char)c;
    }

    return start;
}

char *strtok(char *str, const char *delim)
{
    const char *spanp;
    int c, sc;
    char *tok;
    static char *last;

    if (str == NULL && (str = last) == NULL) {
        return (NULL);
    }

cont:
    c = *str++;
    for (spanp = delim; (sc = *spanp++) != 0;) {
        if (c == sc) {
            goto cont;
        }
    }

    if (c == 0) {
        last = NULL;

        return (NULL);
    }
    tok = str - 1;

    for (;;) {
        c     = *str++;
        spanp = delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0) {
                    str = NULL;
                } else {
                    str[-1] = 0;
                }
                last = str;

                return (tok);
            }
        } while (sc != 0);
    }
}

char *trim(char *str)
{
    size_t len   = 0;
    char *frontp = str;
    char *endp   = NULL;

    if (str == NULL) {
        return NULL;
    }
    if (str[0] == '\0') {
        return str;
    }

    len  = strlen(str);
    endp = str + len;

    /* Move the front and back pointers to address the first non-whitespace
     * characters from each end.
     */
    while (isspace((unsigned char)*frontp)) {
        ++frontp;
    }
    if (endp != frontp) {
        while (isspace((unsigned char)*(--endp)) && endp != frontp) {}
    }
    if (str + len - 1 != endp) {
        *(endp + 1) = '\0';
    } else if (frontp != str && endp == frontp) {
        *str = '\0';
    }
    /* Shift the string so that it starts at str so that if it's dynamically
     * allocated, we can still free it on the returned pointer.  Note the reuse
     * of endp to mean the front of the string buffer now.
     */
    endp = str;
    if (frontp != str) {
        while (*frontp) {
            *endp++ = *frontp++;
        }
        *endp = '\0';
    }

    return str;
}

char *strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *new  = malloc(len);
    if (new == NULL)
        return NULL;
    new[len] = '\0';
    return (char *)memcpy(new, s, len);
}

char *strndup(const char *s, size_t n)
{
    size_t len = strnlen(s, n);
    char *new  = malloc(len);
    if (new == NULL)
        return NULL;
    new[len] = '\0';
    return (char *)memcpy(new, s, len);
}

char *strsep(char **stringp, const char *delim)
{
    char *s;
    const char *spanp;
    int c, sc;
    char *tok;
    if ((s = *stringp) == NULL) {
        return (NULL);
    }
    for (tok = s;;) {
        c     = *s++;
        spanp = delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0) {
                    s = NULL;
                } else {
                    s[-1] = 0;
                }
                *stringp = s;

                return (tok);
            }
        } while (sc != 0);
    }
}

char *itoa(char *buffer, unsigned int num, unsigned int base)
{
    // int numval;
    char *p, *pbase;

    p = pbase = buffer;

    if (base == 16) {
        sprintf(buffer, "%0x", num);
    } else {
        if (num == 0) {
            *p++ = '0';
        }
        while (num != 0) {
            *p++ = (char)('0' + (num % base));
            num  = num / base;
        }
        *p-- = 0;

        while (p > pbase) {
            char tmp;
            tmp    = *p;
            *p     = *pbase;
            *pbase = tmp;

            p--;
            pbase++;
        }
    }
    return buffer;
}

char *replace_char(char *str, char find, char replace)
{
    char *current_pos = strchr(str, find);

    while (current_pos) {
        *current_pos = replace;
        current_pos  = strchr(current_pos, find);
    }

    return str;
}

void strmode(mode_t mode, char *p)
{
    // Usr.
    *p++ = mode & S_IRUSR ? 'r' : '-';
    *p++ = mode & S_IWUSR ? 'w' : '-';
    *p++ = mode & S_IXUSR ? 'x' : '-';
    // Group.
    *p++ = mode & S_IRGRP ? 'r' : '-';
    *p++ = mode & S_IWGRP ? 'w' : '-';
    *p++ = mode & S_IXGRP ? 'x' : '-';
    // Other.
    *p++ = mode & S_IROTH ? 'r' : '-';
    *p++ = mode & S_IWOTH ? 'w' : '-';
    *p++ = mode & S_IXOTH ? 'x' : '-';
    // Will be a '+' if ACL's implemented.
    *p++ = ' ';
    *p   = '\0';
}
