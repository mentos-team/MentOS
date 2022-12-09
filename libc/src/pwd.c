/// @file pwd.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "pwd.h"
#include "sys/unistd.h"
#include "sys/errno.h"
#include "assert.h"
#include "string.h"
#include "stdio.h"
#include "fcntl.h"
#include "debug.h"

static inline void __parse_line(passwd_t *pwd, char *buf)
{
    assert(pwd && "Received null pwd!");
    char *token, *ch;
    // Parse the username.
    if ((token = strtok(buf, ":")) != NULL)
        pwd->pw_name = token;
    // Parse the password.
    if ((token = strtok(NULL, ":")) != NULL)
        pwd->pw_passwd = token;
    // Parse the user ID.
    if ((token = strtok(NULL, ":")) != NULL)
        pwd->pw_uid = atoi(token);
    // Parse the group ID.
    if ((token = strtok(NULL, ":")) != NULL)
        pwd->pw_gid = atoi(token);
    // Parse the user information.
    if ((token = strtok(NULL, ":")) != NULL)
        pwd->pw_gecos = token;
    // Parse the dir.
    if ((token = strtok(NULL, ":")) != NULL)
        pwd->pw_dir = token;
    // Parse the shell.
    if ((token = strtok(NULL, ":")) != NULL) {
        pwd->pw_shell = token;
        // Find carriege return.
        if ((ch = strchr(pwd->pw_shell, '\r')))
            *ch = 0;
        // Find newline.
        if ((ch = strchr(pwd->pw_shell, '\n')))
            *ch = 0;
    }
}

/// @brief Reads a line from the file.
/// @param fd the file descriptor.
/// @param buffer the buffer where we place the line.
/// @param buflen the length of the buffer.
/// @return the amount we read.
ssize_t __readline(int fd, char *buffer, size_t buflen)
{
    memset(buffer, 0, buflen);
    long num_read = read(fd, buffer, buflen);
    if (num_read == 0) {
        return 0;
    }
    char *newline = strchr(buffer, '\n');
    if (newline == NULL) {
        newline = strchr(buffer, EOF);
        if (newline == NULL) {
            newline = strchr(buffer, 0);
            if (newline == NULL) {
                return 0;
            }
        }
    }
    long newline_len = (int)(newline - buffer);
    if (newline_len <= 0) {
        return 0;
    }
    buffer[newline_len] = 0;
    long rollback       = newline_len - num_read + 1;
    if (rollback > 1) {
        return 0;
    }
    lseek(fd, rollback, SEEK_CUR);
    return newline_len;
}

/// @brief Searches for the given entry inside the buffer.
/// @param fd the file descriptor of the file.
/// @param buffer the support buffer we use to read the file.
/// @param buflen the length of the support buffer.
/// @param name the username we are looking for.
/// @param uid the user-id of the user we are looking for.
/// @return the buffer itself if we have found the entry, NULL otherwise.
static inline char *__search_entry(int fd, char *buffer, int buflen, const char *name, uid_t uid)
{
    while (__readline(fd, buffer, buflen)) {
        if (name != NULL) {
            char *name_end = strchr(buffer, ':');
            if (name_end) {
                *name_end = '\0';
                if (strncmp(buffer, name, strlen(name)) == 0) {
                    *name_end = ':';
                    return buffer;
                }
            }
        } else {
            // Name
            char *ptr = strchr(buffer, ':');
            if (ptr == NULL)
                continue;
            // Password
            ++ptr;
            char *uid_start = strchr(ptr, ':');
            if (uid_start == NULL)
                continue;
            ++uid_start;
            ptr = strchr(uid_start, ':');
            if (ptr == NULL)
                continue;
            *ptr = '\0';
            // Parse the uid.
            int found_uid = atoi(uid_start);
            // Check the uid.
            if (found_uid == uid) {
                *ptr = ':';
                return buffer;
            }
        }
    }
    return NULL;
}

passwd_t *getpwnam(const char *name)
{
    if (name == NULL)
        return NULL;
    static passwd_t pwd;
    static char buffer[BUFSIZ];
    passwd_t *result;
    if (!getpwnam_r(name, &pwd, buffer, BUFSIZ, &result))
        return NULL;
    return &pwd;
}

passwd_t *getpwuid(uid_t uid)
{
    static passwd_t pwd;
    static char buffer[BUFSIZ];
    passwd_t *result;
    if (!getpwuid_r(uid, &pwd, buffer, BUFSIZ, &result))
        return NULL;
    return &pwd;
}

int getpwnam_r(const char *name, passwd_t *pwd, char *buf, size_t buflen, passwd_t **result)
{
    if (name == NULL)
        return 0;
    int fd = open("/etc/passwd", O_RDONLY, 0);
    if (fd == -1) {
        pr_debug("Cannot open `/etc/passwd`\n");
        errno   = ENOENT;
        *result = NULL;
        return 0;
    }
    char *entry = __search_entry(fd, buf, buflen, name, 0);
    if (entry != NULL) {
        // Close the file.
        close(fd);
        // Parse the line.
        __parse_line(pwd, entry);
        // Return success.
        return 1;
    }
    errno = ENOENT;
    // Close the file.
    close(fd);
    // Return fail.
    return 0;
}

int getpwuid_r(uid_t uid, passwd_t *pwd, char *buf, size_t buflen, passwd_t **result)
{
    int fd = open("/etc/passwd", O_RDONLY, 0);
    if (fd == -1) {
        pr_debug("Cannot open `/etc/passwd`\n");
        errno   = ENOENT;
        *result = NULL;
        return 0;
    }
    char *entry = __search_entry(fd, buf, buflen, NULL, uid);
    if (entry != NULL) {
        // Close the file.
        close(fd);
        // Parse the line.
        __parse_line(pwd, entry);
        // Return success.
        return 1;
    }
    // Close the file.
    close(fd);
    // Return fail.
    return 0;
}
