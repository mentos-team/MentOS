/// @file grp.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "grp.h"
#include "assert.h"
#include "errno.h"
#include "fcntl.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"

/// Holds the file descriptor while we are working with `/etc/group`.
static int __fd = -1;

/// @brief It parses the line (as string) and saves its content inside the
/// group_t structure.
/// @param grp the struct where we store the information.
/// @param buf the line from which we extract the information.
static inline void __parse_line(group_t *grp, char *buf)
{
    assert(grp && "Received null grp!");
    char *token;
    // Parse the group name.
    token = strtok(buf, ":");
    if (token != NULL) {
        grp->gr_name = token;
    }
    // Parse the group passwd.
    token = strtok(NULL, ":");
    if (token != NULL) {
        grp->gr_passwd = token;
    }
    // Parse the group id.
    token = strtok(NULL, ":");
    if (token != NULL) {
        grp->gr_gid = atoi(token);
    }
    size_t found_users = 0;
    while ((token = strtok(NULL, ",\n\0")) != NULL && found_users < MAX_MEMBERS_PER_GROUP) {
        grp->gr_mem[found_users] = token;
        found_users += 1;
    }
    // Null terminate array
    grp->gr_mem[found_users] = "\0";
}

/// @brief Searches an entry in `/etc/group`.
/// @param fd the file descriptor pointing to `/etc/group`.
/// @param buf the buffer we are going to use to search the entry.
/// @param buflen the length of the buffer.
/// @param name the name we are looking for.
/// @param gid the group id we must match.
/// @return 1 on success, 0 on failure.
static inline int __search_entry(int fd, char *buf, size_t buflen, const char *name, gid_t gid)
{
    int ret;
    char c;
    int pos = 0;
    while ((ret = read(fd, &c, 1U))) {
        // Skip carriage return.
        if (c == '\r') {
            continue;
        }
        if (pos >= buflen) {
            errno = ERANGE;
            return 0;
        }
        // If we have found a newline or the EOF, parse the entry.
        if ((c == '\n') || (ret == EOF)) {
            // Close the buffer.
            buf[pos] = 0;
            // Check the entry.
            if (name) {
                if (strncmp(buf, name, strlen(name)) == 0 && buf[strlen(name)] == ':') {
                    return 1;
                }
            } else {
                int gid_start = -1;
                int col_count = 0;
                for (int i = 0; i < pos; ++i) {
                    if (buf[i] == ':') {
                        if (++col_count == 2) {
                            gid_start = i + 1;
                            break;
                        }
                    }
                }
                if ((gid_start != -1) && (gid_start < pos)) {
                    // Parse the gid.
                    int found_gid = atoi(&buf[gid_start]);
                    // Check the gid.
                    if (found_gid == gid) {
                        return 1;
                    }
                }
            }
            // Reset the index.
            pos = 0;
            // If we have reached the EOF stop.
            if (ret == EOF) {
                break;
            }
        } else {
            buf[pos++] = c;
        }
    }
    errno = ENOENT;
    return 0;
}

group_t *getgrgid(gid_t gid)
{
    static group_t grp;
    static char buffer[BUFSIZ];

    group_t *result;
    if (!getgrgid_r(gid, &grp, buffer, BUFSIZ, &result)) {
        return NULL;
    }

    return &grp;
}

group_t *getgrnam(const char *name)
{
    if (name == NULL) {
        return NULL;
    }

    static group_t grp;
    static char buffer[BUFSIZ];

    group_t *result;
    if (!getgrnam_r(name, &grp, buffer, BUFSIZ, &result)) {
        return NULL;
    }

    return &grp;
}

int getgrgid_r(gid_t gid, group_t *group, char *buf, size_t buflen, group_t **result)
{
    int fd = open("/etc/group", O_RDONLY, 0);
    if (fd == -1) {
        errno   = ENOENT;
        *result = NULL;
        return 0;
    }

    if (__search_entry(fd, buf, buflen, NULL, gid)) {
        // Close the file.
        close(fd);
        // Parse the line.
        __parse_line(group, buf);
        // Return success.
        return 1;
    }

    // Close the file.
    close(fd);
    // Return fail.
    return 0;
}

int getgrnam_r(const char *name, group_t *group, char *buf, size_t buflen, group_t **result)
{
    int fd = open("/etc/group", O_RDONLY, 0);
    if (fd == -1) {
        errno   = ENOENT;
        *result = NULL;
        return 0;
    }

    if (__search_entry(fd, buf, buflen, name, 0)) {
        // Close the file.
        close(fd);
        // Parse the line.
        __parse_line(group, buf);
        // Return success.
        return 1;
    }

    // Close the file.
    close(fd);
    // Return fail.
    return 0;
}

group_t *getgrent(void)
{
    static group_t result;

    if (__fd == -1) {
        //pr_debug("Opening group file\n");
        __fd = open("/etc/group", O_RDONLY, 0);
        if (__fd == -1) {
            errno = ENOENT;
            return 0;
        }
    }

    int ret;
    char c;
    int pos = 0;

    static char buffer[BUFSIZ];
    while ((ret = read(__fd, &c, 1U))) {
        // Skip carriage return.
        if (c == '\r') {
            continue;
        }

        if (pos >= BUFSIZ) {
            errno = ERANGE;
            return NULL;
        }

        // If we have found a newline or the EOF, parse the entry.
        if ((c == '\n') || (ret == EOF)) {
            // Close the buffer.
            buffer[pos] = 0;

            // Check the entry.
            if (strlen(buffer) != 0) {
                //pr_debug("Found entry in group file: %s\n", buffer);
                __parse_line(&result, buffer);
                return &result;
            }

            // If we have reached the EOF stop.
            if (ret == EOF) {
                break;
            }

        } else {
            buffer[pos++] = c;
        }
    }

    errno = ENOENT;
    return NULL;
}

void endgrent(void)
{
    //pr_debug("Closing group file\n");
    close(__fd);
    __fd = -1;
}

void setgrent(void)
{
    //pr_debug("Resetting pointer to beginning of group file\n");
    lseek(__fd, 0, SEEK_SET);
}
