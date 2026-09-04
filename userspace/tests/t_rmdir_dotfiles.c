/// @file t_rmdir_dotfiles.c
/// @brief Regression test for #341: rmdir must refuse a directory that still
/// holds files, whatever their names look like.
/// @details `ext2_directory_is_empty` skipped `.` and `..` by comparing a
/// prefix rather than the name:
///
///     if (!strncmp(it.direntry->name, ".", 1) || !strncmp(it.direntry->name, "..", 2))
///
/// `strncmp(name, ".", 1)` compares one character, so it was true for every
/// dot-prefixed name. A directory holding only `.bashrc`, or `.config`, or
/// `..weird`, was reported empty; rmdir removed its entry from the parent and
/// its contents were left with no path at all, with inodes whose links_count
/// stayed above zero so nothing would ever free them, and data blocks marked
/// used for the life of the filesystem.
///
/// Dot-prefixed files are where a Unix system keeps per-user configuration,
/// so this lost exactly the kind of file a home directory is full of.
///
/// Each refusal is paired with the controls that removal still works, because
/// the cheapest way to pass a test like this is to stop removing directories
/// at all.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

/// The directory the checks build and remove.
#define DIR "/home/user/t_rmdir_dotfiles.d"

/// @brief Builds DIR/<name>.
/// @param out where the path is stored.
/// @param size the size of the output buffer.
/// @param name the file name to append.
static void __path_of(char *out, size_t size, const char *name)
{
    strncpy(out, DIR "/", size - 1);
    out[size - 1] = 0;
    strncat(out, name, size - strlen(out) - 1);
}

/// @brief Checks that a directory holding one file cannot be removed.
/// @param name the name of the file to put in it.
/// @return 0 when rmdir refused, -1 otherwise.
static int __check_refused(const char *name)
{
    char path[128];
    __path_of(path, sizeof(path), name);

    if (mkdir(DIR, 0755) < 0) {
        syslog(LOG_ERR, "[t_rmdir_dotfiles] mkdir for `%s`: %s", name, strerror(errno));
        return -1;
    }
    int fd = creat(path, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_rmdir_dotfiles] creating `%s`: %s", name, strerror(errno));
        rmdir(DIR);
        return -1;
    }
    close(fd);

    errno   = 0;
    int ret = rmdir(DIR);
    if (ret == 0) {
        // The entry is gone and the file with it: report whether anything can
        // still reach what was lost, because that is the damage.
        errno    = 0;
        int lost = open(path, O_RDONLY, 0);
        syslog(
            LOG_ERR, "[t_rmdir_dotfiles] a directory holding `%s` was removed; reopening the file gives errno %d", name,
            errno);
        if (lost >= 0) {
            close(lost);
        }
        return -1;
    }
    int failures = 0;
    if (errno != ENOTEMPTY) {
        int reported = errno;
        syslog(LOG_ERR, "[t_rmdir_dotfiles] rmdir with `%s` inside reported errno %d: %s", name, reported, strerror(reported));
        syslog(LOG_ERR, "[t_rmdir_dotfiles] the expected errno was %d: %s", ENOTEMPTY, strerror(ENOTEMPTY));
        ++failures;
    }
    // Clean up, and prove the directory really was still there to remove.
    if (unlink(path) < 0) {
        syslog(LOG_ERR, "[t_rmdir_dotfiles] unlink of `%s`: %s", name, strerror(errno));
        ++failures;
    }
    if (rmdir(DIR) < 0) {
        syslog(LOG_ERR, "[t_rmdir_dotfiles] rmdir after emptying (`%s`): %s", name, strerror(errno));
        ++failures;
    }
    return (failures == 0) ? 0 : -1;
}

/// @brief Control: an empty directory is still removable.
/// @return 0 on success, -1 on failure.
static int __check_empty_removable(void)
{
    if (mkdir(DIR, 0755) < 0) {
        syslog(LOG_ERR, "[t_rmdir_dotfiles] mkdir of the empty case: %s", strerror(errno));
        return -1;
    }
    if (rmdir(DIR) < 0) {
        syslog(LOG_ERR, "[t_rmdir_dotfiles] an empty directory could not be removed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;

    // The control for the refusals: an ordinary name was always refused
    // correctly, so a failure here is not about the dot at all.
    if (__check_refused("plain") < 0) {
        ++failures;
    }
    // One dot: the name the broken one-character comparison collided with.
    if (__check_refused(".hidden") < 0) {
        ++failures;
    }
    // Two dots and more: collided with the `..` comparison as well.
    if (__check_refused("..weird") < 0) {
        ++failures;
    }
    // And the other control: removal still works when it should.
    if (__check_empty_removable() < 0) {
        ++failures;
    }

    if (failures == 0) {
        syslog(LOG_INFO, "[t_rmdir_dotfiles] rmdir refuses a directory that still holds any file");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_rmdir_dotfiles] %d FAILURES", failures);
    return EXIT_FAILURE;
}
