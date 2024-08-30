/// @file namei.c
/// @brief Implementation of functions fcntl() and open().
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[NAMEI ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "fs/namei.h"
#include "assert.h"
#include "limits.h"
#include "fcntl.h"
#include "sys/stat.h"
#include "fs/vfs.h"
#include "process/scheduler.h"
#include "sys/errno.h"
#include "strerror.h"
#include "string.h"

/// Appends the path with a "/" as separator.
#define APPEND_PATH_SEPARATOR(buffer, buflen)                                             \
    {                                                                                     \
        if (buffer[strnlen(buffer, buflen) - 1] != '/') { strncat(buffer, "/", buflen); } \
    }

/// Appends the path.
#define APPEND_PATH(buffer, token)             \
    {                                          \
        strncat(buffer, token, strlen(token)); \
    }

int sys_unlink(const char *path)
{
    return vfs_unlink(path);
}

int sys_mkdir(const char *path, mode_t mode)
{
    return vfs_mkdir(path, mode);
}

int sys_rmdir(const char *path)
{
    return vfs_rmdir(path);
}

int sys_creat(const char *path, mode_t mode)
{
    // Get the current task.
    task_struct *task = scheduler_get_current_process();

    // Search for an unused fd.
    int fd = get_unused_fd();
    if (fd < 0)
        return fd;

    // Try to open the file.
    vfs_file_t *file = vfs_creat(path, mode);
    if (file == NULL) {
        return -errno;
    }

    // Set the file descriptor id.
    task->fd_list[fd].file_struct = file;
    task->fd_list[fd].flags_mask  = O_WRONLY | O_CREAT | O_TRUNC;

    // Return the file descriptor and increment it.
    return fd;
}

int sys_symlink(const char *linkname, const char *path)
{
    return vfs_symlink(linkname, path);
}

int sys_readlink(const char *path, char *buffer, size_t bufsize)
{
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // Resolve the path.
    int ret = resolve_path(path, absolute_path, sizeof(absolute_path), 0);
    if (ret < 0) {
        pr_err("sys_readlink(%s): Cannot resolve path!\n", path);
        return ret;
    }
    // Read the link.
    ssize_t nbytes = vfs_readlink(path, buffer, bufsize);
    // Return the number of bytes we read.
    return nbytes;
}

char *realpath(const char *path, char *buffer, size_t buflen)
{
    int ret = resolve_path(path, buffer, buflen, REMOVE_TRAILING_SLASH);
    if (ret < 0) {
        errno = -ret;
        return NULL;
    }
    return buffer;
}

/// @brief Determines if the path points to a link.
/// @param path the path to the file.
/// @return 1 if it is a link, 0 otherwise.
static inline int __is_a_link(const char *path)
{
    stat_t statbuf;
    if (vfs_stat(path, &statbuf) > 0) {
        return S_ISLNK(statbuf.st_mode);
    }
    return 0;
}

/// @brief Returns the content of the link.
/// @param path the path to the file.
/// @param buffer the buffer where we store the link content.
/// @param buflen length of the buffer.
/// @return the length of the link, or a negative value on error.
static inline int __get_link_content(const char *path, char *buffer, size_t buflen)
{
    ssize_t link_length = vfs_readlink(path, buffer, buflen);
    if (link_length < 0) {
        return link_length;
    }
    // Null-terminate link.
    buffer[link_length] = 0;
    return link_length;
}

/// @brief Resolve the path by following all symbolic links.
/// @param path the path to resolve.
/// @param buffer the buffer where the resolved path is stored.
/// @param buflen the size of the provided resolved_path buffer.
/// @param flags the flags controlling how the path is resolved.
/// @param link_depth the current link depth.
/// @return -errno on fail, 1 on success.
int __resolve_path(const char *path, char *abspath, size_t buflen, int flags, int link_depth)
{
    char token[NAME_MAX]    = { 0 };
    char buffer[PATH_MAX]   = { 0 };
    char linkpath[PATH_MAX] = { 0 };
    size_t offset = 0, linklen = 0, tokenlen = 0;
    int contains_links = 0;
    stat_t statbuf;

    if (path[0] != '/') {
        // Get the working directory of the current task.
        sys_getcwd(buffer, buflen);
        pr_debug("|%-32s|%-32s| (INIT)\n", path, buffer);
    } else {
        pr_debug("|%-32s|%-32s| (INIT)\n", path, buffer);
    }

    while (tokenize(path, "/", &offset, token, NAME_MAX)) {
        tokenlen = strlen(token);
        if ((strcmp(token, "..") == 0) && (tokenlen == 2)) {
            // Handle parent directory token "..".
            if (strlen(buffer) > 0) {
                // Find the last occurrence of '/'.
                char *last_slash = strrchr(buffer, '/');
                if (!last_slash || (last_slash == buffer)) {
                    // This case handles if buffer is already empty (e.g., ".."
                    // from the root).
                    buffer[0] = '/';
                    buffer[1] = 0;
                    pr_debug("|%-32s|%-32s| (RESET)\n", path, buffer);
                } else {
                    // Truncate at the last slash.
                    *last_slash = '\0';
                    pr_debug("|%-32s|%-32s| (TRUNCATE)\n", path, buffer);
                }
            }
        } else if ((strcmp(token, ".") == 0) && (tokenlen == 1)) {
            // Nothing to do.
        } else {
            if (strlen(buffer) + tokenlen + 1 < buflen) {
                APPEND_PATH_SEPARATOR(buffer, buflen);
                APPEND_PATH(buffer, token);
                pr_debug("|%-32s|%-32s|%d| (APPEND)\n", path, buffer, tokenlen);
            } else {
                pr_err("Buffer overflow while resolving path.\n");
                return -ENAMETOOLONG;
            }
            if ((flags & FOLLOW_LINKS) && __is_a_link(buffer)) {
                ssize_t link_length = __get_link_content(buffer, linkpath, PATH_MAX);
                if (link_length > 0) {
                    if (link_depth >= SYMLOOP_MAX) {
                        pr_err("Reached symbolic link maximum depth `%d`.\n", link_depth);
                        return -ELOOP;
                    }
                    linklen = strlen(linkpath);

                    if (linkpath[0] == '/') {
                        memcpy(buffer, linkpath, linklen);
                        pr_debug("|%-32s|%-32s| (REPLACE)\n", path, buffer);
                    } else {
                        // Find the last occurrence of '/'.
                        char *last_slash = strrchr(buffer, '/');
                        if (last_slash) {
                            memcpy(++last_slash, linkpath, linklen);
                            pr_debug("|%-32s|%-32s|%-32s| (LINK)\n", path, buffer, linkpath);
                        }
                    }
                    contains_links = 1;
                }
            }
        }
    }
    if (contains_links) {
        pr_debug("|%-32s|%-32s| (RECU)\n", path, buffer);
        return __resolve_path(buffer, abspath, buflen, flags, ++link_depth);
    }
    // Get the end of the buffer.
    size_t buffer_end = strnlen(buffer, buflen);
    // If the buffer is empty, set it to '/', or remove trailing slash if requested.
    if (buffer_end == 0) {
        buffer[0] = '/';
        buffer[1] = 0;
    } else if ((flags & REMOVE_TRAILING_SLASH) && (buffer_end > 1) && (buffer[buffer_end - 1] == '/')) {
        pr_debug("|%-32s|%-32s|(%u) (REMTRAIL)\n", path, buffer, buffer_end);
        buffer[buffer_end] = 0;
    }
    strncpy(abspath, buffer, buflen);
    pr_debug("|%-32s|%-32s|(%u) (END)\n", path, buffer, buffer_end);
    return 0;
}

int resolve_path(const char *path, char *abspath, size_t buflen, int flags)
{
    return __resolve_path(path, abspath, buflen, flags, 0);
}
