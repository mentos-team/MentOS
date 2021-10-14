///                MentOS, The Mentoring Operating system project
/// @file pwd.c
/// @brief
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "pwd.h"
#include "sys/unistd.h"
#include "sys/errno.h"
#include "assert.h"
#include "string.h"
#include "stdio.h"
#include "fcntl.h"

static inline void __parse_line(passwd_t *pwd, char *buf)
{
    assert(pwd && "Received null pwd!");
    char *token;
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
    if ((token = strtok(NULL, ":")) != NULL)
        pwd->pw_shell = token;
}

static inline char *__search_entry(int fd, char *buf, int buflen, const char *name, uid_t uid)
{
    int ret;
    char c;
    int pos = 0;
    while ((ret = read(fd, &c, 1U))) {
        // Skip carriage return.
        if (c == '\r')
            continue;
        if (pos >= buflen) {
            errno = ERANGE;
            return NULL;
        }
        // If we have found a newline or the EOF, parse the entry.
        if ((c == '\n') || (ret == EOF)) {
            // Close the buffer.
            buf[pos] = 0;
            // Check the entry.
            if (name) {
                if (strncmp(buf, name, strlen(name)) == 0)
                    return buf;
            } else {
                int uid_start = -1, col_count = 0;
                for (int i = 0; i < pos; ++i) {
                    if (buf[i] == ':') {
                        if (++col_count == 2) {
                            uid_start = i + 1;
                            break;
                        }
                    }
                }
                if ((uid_start != -1) && (uid_start < pos)) {
                    // Parse the uid.
                    int found_uid = atoi(&buf[uid_start]);
                    // Check the uid.
                    if (found_uid == uid)
                        return buf;
                }
            }
            // Reset the index.
            pos = 0;
            // If we have reached the EOF stop.
            if (ret == EOF)
                break;
        } else {
            buf[pos++] = c;
        }
    }
    errno = ENOENT;
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
    // Close the file.
    close(fd);
    // Return fail.
    return 0;
}

int getpwuid_r(uid_t uid, passwd_t *pwd, char *buf, size_t buflen, passwd_t **result)
{
    int fd = open("/etc/passwd", O_RDONLY, 0);
    if (fd == -1) {
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
