/// @file namei.c
/// @brief Implementation of functions fcntl() and open().
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[NAMEI ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "assert.h"
#include "errno.h"
#include "fcntl.h"
#include "fs/namei.h"
#include "fs/vfs.h"
#include "limits.h"
#include "process/scheduler.h"
#include "strerror.h"
#include "string.h"
#include "sys/stat.h"

/// @brief Appends the "/" separator to the path being built, unless the
///        path already ends with it.
/// @param buffer the path being built, always NUL-terminated.
/// @param buflen the capacity of the buffer.
/// @details An empty path needs the leading separator, and reading the
///          last byte must never index before the beginning of the
///          buffer: the empty case used to be decided by the byte that
///          happened to precede the buffer, a read the caller did not
///          own (#376).
static inline void append_path_separator(char *buffer, size_t buflen)
{
    size_t length = strnlen(buffer, buflen);
    if ((length == 0) || (buffer[length - 1] != '/')) {
        strncat(buffer, "/", buflen - length - 1);
    }
}

/// Appends the path.
#define APPEND_PATH(buffer, token)             \
    {                                          \
        strncat(buffer, token, strlen(token)); \
    }

int sys_unlink(const char *path) { return vfs_unlink(path); }

int sys_mkdir(const char *path, mode_t mode) { return vfs_mkdir(path, mode); }

int sys_rmdir(const char *path) { return vfs_rmdir(path); }

int sys_creat(const char *path, mode_t mode)
{
    // Get the current task.
    task_struct *task = scheduler_get_current_process();

    // Search for an unused fd.
    int fd = get_unused_fd();
    if (fd < 0) {
        return fd;
    }

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

int sys_symlink(const char *linkname, const char *path) { return vfs_symlink(linkname, path); }

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
/// @return 1 if it is a link, 0 if it is not, -errno if it could not be told.
/// @details A component that is not there is not a link, and resolution has to
///          walk past it: the last component of a path being created does not
///          exist yet. A component whose inode could not be read is a
///          different answer. This used to be one answer for both, because a
///          failed `vfs_stat` came back as 0, so a symbolic link that could
///          not be read was resolved as if it were an ordinary name and the
///          operation landed on whatever the link would have redirected away
///          from — a different file from the one the caller named (#353).
static inline int __is_a_link(const char *path)
{
    stat_t statbuf;
    int err = vfs_stat(path, &statbuf);
    if (err == 0) {
        return S_ISLNK(statbuf.st_mode);
    }
    // Absent is a definite answer, and the only one that lets the walk go on.
    if (err == -ENOENT) {
        return 0;
    }
    return err;
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
    // The read may report a target as long as the whole buffer, or longer
    // than what it actually terminated (#371); clamping keeps the
    // terminator write below in bounds and the buffer terminated.
    if (link_length >= (ssize_t)buflen) {
        link_length = buflen - 1;
    }
    // Null-terminate link.
    buffer[link_length] = 0;
    return link_length;
}

/// @brief Resolve the path by following all symbolic links.
/// @param path the path to resolve.
/// @param abspath the buffer where the resolved path is stored.
/// @param buflen the size of the provided resolved_path buffer.
/// @param flags the flags controlling how the path is resolved.
/// @param link_depth the current link depth.
/// @return -errno on fail, 1 on success.
int __resolve_path(const char *path, char *abspath, size_t buflen, int flags, int link_depth)
{
    char token[NAME_MAX]    = {0};
    char buffer[PATH_MAX]   = {0};
    char linkpath[PATH_MAX] = {0};
    size_t offset           = 0;
    size_t linklen          = 0;
    size_t tokenlen         = 0;
    int contains_links      = 0;

    if (path[0] != '/') {
        // Get the working directory of the current task.
        sys_getcwd(buffer, buflen);
        pr_debug("|%-32s|%-32s| (INIT)\n", path, buffer);
    } else {
        pr_debug("|%-32s|%-32s| (INIT)\n", path, buffer);
    }

    int tokens;
    while ((tokens = tokenize(path, "/", &offset, token, NAME_MAX)) > 0) {
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
                append_path_separator(buffer, buflen);
                APPEND_PATH(buffer, token);
                pr_debug("|%-32s|%-32s|%d| (APPEND)\n", path, buffer, tokenlen);
            } else {
                pr_err("Buffer overflow while resolving path.\n");
                return -ENAMETOOLONG;
            }
            // Only ask when links are being followed, so a resolution that
            // does not care about them cannot fail on a component it was
            // never going to read.
            int is_link = (flags & FOLLOW_LINKS) ? __is_a_link(buffer) : 0;
            if (is_link < 0) {
                pr_err("Cannot tell whether `%s` is a symbolic link (%d).\n", buffer, is_link);
                return is_link;
            }
            if (is_link) {
                ssize_t link_length = __get_link_content(buffer, linkpath, PATH_MAX);
                if (link_length > 0) {
                    if (link_depth >= SYMLOOP_MAX) {
                        pr_err("Reached symbolic link maximum depth `%d`.\n", link_depth);
                        return -ELOOP;
                    }
                    linklen = strlen(linkpath);

                    // An absolute target replaces the whole path built so
                    // far, a relative one replaces the component after the
                    // last slash. Both used to copy `linklen` bytes with no
                    // bound, writing past the end of the buffer for a target
                    // that did not fit, and no terminator, leaving the bytes
                    // of the replaced path trailing the link content (#288).
                    size_t dst = 0;
                    if (linkpath[0] != '/') {
                        // Find the last occurrence of '/'.
                        char *last_slash = strrchr(buffer, '/');
                        dst              = last_slash ? (size_t)(last_slash - buffer) + 1 : 0;
                    }
                    // The same fit rule as the append path: the substituted
                    // content must leave room for the terminator.
                    if (dst + linklen >= buflen) {
                        pr_err("Link substitution overflows the path buffer.\n");
                        return -ENAMETOOLONG;
                    }
                    memcpy(buffer + dst, linkpath, linklen);
                    buffer[dst + linklen] = 0;
                    if (dst == 0) {
                        pr_debug("|%-32s|%-32s| (REPLACE)\n", path, buffer);
                    } else {
                        pr_debug("|%-32s|%-32s|%-32s| (LINK)\n", path, buffer, linkpath);
                    }
                    contains_links = 1;
                }
            }
        }
    }
    // A component that does not fit the token buffer is rejected, never
    // truncated: a truncated component would resolve to a different name.
    if (tokens < 0) {
        pr_err("Path component too long while resolving a path.\n");
        return -ENAMETOOLONG;
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

int resolve_path(const char *path, char *buffer, size_t buflen, int flags)
{
    return __resolve_path(path, buffer, buflen, flags, 0);
}
