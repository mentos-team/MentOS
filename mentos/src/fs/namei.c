/// @file namei.c
/// @brief Implementation of functions fcntl() and open().
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "assert.h"
#include "limits.h"
#include "fs/namei.h"
#include "fcntl.h"
#include "fs/vfs.h"
#include "io/debug.h"
#include "process/scheduler.h"
#include "sys/errno.h"
#include "string.h"

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
    task->fd_list[fd].flags_mask = O_WRONLY|O_CREAT|O_TRUNC;

    // Return the file descriptor and increment it.
    return fd;
}

int sys_symlink(const char *linkname, const char *path)
{
    return vfs_symlink(linkname, path);
}

int sys_readlink(const char *path, char *buffer, size_t bufsize)
{
    // Try to open the file.
    vfs_file_t *file = vfs_open(path, O_RDONLY, 0);
    if (file == NULL) {
        return -errno;
    }
    // Read the link.
    ssize_t nbytes = vfs_readlink(file, buffer, bufsize);
    // Close the file.
    vfs_close(file);
    // Return the number of bytes we read.
    return nbytes;
}

char *realpath(const char *path, char *buffer, size_t buflen) {
    int ret = resolve_path(path, buffer, buflen, REMOVE_TRAILING_SLASH);
    if (ret < 0) {
        errno = -ret;
        return NULL;
    }
    return buffer;
}

#define APPEND_PATH_SEP_OR_FAIL(b, remaining) \
{                                             \
    strncat(b, "/", remaining);               \
    remaining--;                              \
    if (remaining < 0)                        \
        return -ENAMETOOLONG;                 \
}

#define APPEND_PATH_OR_FAIL(b, path, remaining) \
{                                               \
    strncat(b, path, remaining);                \
    remaining -= strlen(path);                  \
    if (remaining < 0)                          \
        return -ENAMETOOLONG;                   \
}

int resolve_path(const char *path, char *buffer, size_t buflen, int flags)
{
    assert(path && "Provided null path.");
    assert(buffer && "Provided null buffer.");

    // Buffer used to build up the absolute path
    char abspath[buflen];
    // Null-terminate our work buffer
    memset(abspath,0, buflen);
    int remaining = buflen - 1;

    // Track the resolved symlinks to ensure we do not end up in a loop
    int symlinks = 0;

    if (path[0] != '/') {
        // Get the working directory of the current task.
        sys_getcwd(abspath, remaining);
        // Check the absolute path.
        assert((strlen(abspath) > 0) && "Current working directory is not set.");
        // Check that the current working directory is an absolute path.
        assert((abspath[0] == '/') && "Current working directory is not an absolute path.");
        // Count the remaining space in the absolute path.
        remaining -= strlen(abspath);
        APPEND_PATH_SEP_OR_FAIL(abspath, remaining);
    }

    // Copy the path into the working buffer;
    APPEND_PATH_OR_FAIL(abspath, path, remaining);

    int absidx, pathidx;
resolve_abspath:
    absidx = pathidx = 0;
    while (abspath[absidx]) {
        // Skip multiple consecutive / characters
        if (!strncmp("//", abspath + absidx, 2)) {
            absidx++;
        }
        // Go to previous directory if /../ is found
        else if (!strncmp("/../", abspath + absidx, 4)) {
            // Go to a valid path character (pathidx points to the next one)
            if (pathidx) {
                pathidx--;
            }
            while (pathidx && buffer[pathidx] != '/') {
                pathidx--;
            }
            absidx += 3;
        } else if (!strncmp("/./", abspath + absidx, 3)) {
            absidx += 2;
        } else {
            // Resolve a possible symlink
            if (flags & FOLLOW_LINKS) {
                // Write the next path separator
                buffer[pathidx++] = abspath[absidx++];

                // Find the next separator after the current path component
                char* sep_after_cur = strchr(abspath + absidx, '/');
                if (sep_after_cur) {
                    // Null-terminate work buffer to properly open the current component
                    *sep_after_cur = 0;
                }

                vfs_file_t *file;
                file = vfs_open_abspath(abspath, O_RDONLY, 0);
                if (!file) { return -errno; }

                stat_t statbuf;
                int ret = vfs_fstat(file, &statbuf);
                if (ret < 0) { vfs_close(file); return ret; }

                // The path component is no symbolic link
                if (!S_ISLNK(statbuf.st_mode)) {
                    vfs_close(file);
                    // Restore replaced path separator
                    *sep_after_cur = '/';
                    // Just copy the component into the buffer
                    goto copy_path_component;
                }

                symlinks++;
                if (symlinks > SYMLOOP_MAX) { return -ELOOP; }

                char link[PATH_MAX];
                ssize_t nbytes = vfs_readlink(file, link, sizeof(link));
                vfs_close(file);
                if (nbytes == -1) { return -errno; }

                // Null-terminate link
                link[nbytes] = 0;

                // Link is an absolute path, replace everything and continue
                if (link[0] == '/') {
                    // Save the rest of the current abspath into the buffer
                    if (sep_after_cur) {
                        strncpy(buffer, sep_after_cur + 1, buflen);
                    }

                    // Reset abspath with link
                    remaining = buflen - 1;
                    strncpy(abspath, link, remaining);

                    remaining -= strlen(link);
                    if (remaining  < 0) { return -ENAMETOOLONG; }

                    // This is not the last component, therefore it must be a directory
                    if (sep_after_cur) {
                        APPEND_PATH_SEP_OR_FAIL(abspath, remaining);
                        // Copy the saved content from buffer back to the abspath
                        APPEND_PATH_OR_FAIL(abspath, buffer, remaining);
                    }
                    goto resolve_abspath;
                // Link is relative, add it and continue
                } else {
                    // Recalculate the remaining space in the working buffer
                    remaining = buflen - absidx - 1;
                    strncpy(abspath + absidx, link, remaining);

                    remaining -= strlen(link);
                    if (remaining < 0) { return -ENAMETOOLONG; }

                    if (sep_after_cur) {
                        // Restore replaced path separator
                        *sep_after_cur = '/';
                    }

                    // Continue with to previous path separator
                    absidx--;
                    pathidx--;
                }

            }
            // Copy the path component
            else {
copy_path_component:
                do {
                    buffer[pathidx++] = abspath[absidx++];
                }
                while (abspath[absidx] && abspath[absidx] != '/');
            }
        }
    }

    // Null-terminate buffer
    buffer[pathidx] = 0;

    if (flags & REMOVE_TRAILING_SLASH) {
        // Ensure the path is not just "/"
        if (pathidx > 1 && buffer[pathidx] == '/') {
            buffer[pathidx - 1] = '\0';
        }
    }

    return 0;
}
