/// @file proc_feedback.c
/// @brief Contains callbacks for procfs system files.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "fs/procfs.h"
#include "io/debug.h"
#include "math.h"
#include "process/process.h"
#include "process/scheduler_feedback.h"
#include "stdio.h"
#include "string.h"

/// @brief Reposition a `/proc/feedback` read offset.
/// @param file The procfs file whose offset is being changed.
/// @param offset Offset relative to @p whence.
/// @param whence `SEEK_SET`, `SEEK_CUR`, or `SEEK_END`.
/// @return The new non-negative offset, or a negative errno.
static off_t procfb_lseek(vfs_file_t *file, off_t offset, int whence)
{
    if (file == NULL) {
        return -EBADF;
    }

    off_t base = 0;
    if (whence == SEEK_CUR) {
        base = file->f_pos;
    } else if (whence == SEEK_END) {
        char support[BUFSIZ];
        memset(support, 0, sizeof(support));
        scheduler_feedback_to_string(support, sizeof(support));
        base = (off_t)strlen(support);
    } else if (whence != SEEK_SET) {
        return -EINVAL;
    }

    if (offset < 0 && offset < -base) {
        return -EINVAL;
    }
    off_t next = base + offset;
    if (next < 0) {
        return -EINVAL;
    }
    file->f_pos = next;
    return next;
}

/// @brief Reads data from the /proc/feedback file.
///
/// @param file A pointer to the vfs_file_t structure representing the file to read from.
/// @param buf A buffer to store the read data.
/// @param offset The offset from where the read operation should begin.
/// @param nbyte The number of bytes to read.
/// @return The number of bytes actually read, or -ENOENT if the file is NULL.
static ssize_t procfb_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    // Check if the file pointer is NULL.
    if (!file) {
        pr_err("procfb_read: Received a NULL file.\n");
        return -ENOENT; // Return an error if the file is NULL.
    }

    // Prepare a support buffer, and format the scheduling feedback into it.
    char support[BUFSIZ];
    memset(support, 0, BUFSIZ);
    scheduler_feedback_to_string(support, BUFSIZ);

    // Keep the subtraction unsigned only after proving that the offset is
    // valid.  Otherwise strlen(support) - offset wraps for an offset beyond
    // EOF and memcpy() reads past the temporary buffer (#454).
    if (offset < 0) {
        return -EINVAL;
    }
    size_t length = strlen(support);
    if ((size_t)offset >= length) {
        return 0;
    }
    ssize_t bytes_to_read = (ssize_t)min(length - (size_t)offset, nbyte);
    // Perform the read: copy exactly the computed amount, never more, since
    // buf is the raw user read(2) buffer and nbyte is all it can hold.
    if (bytes_to_read > 0) {
        memcpy(buf, support + offset, (size_t)bytes_to_read);
    }
    return bytes_to_read;
}

/// Filesystem general operations.
static vfs_sys_operations_t procfb_sys_operations = {
    .mkdir_f   = NULL,
    .rmdir_f   = NULL,
    .stat_f    = NULL,
    .creat_f   = NULL,
    .symlink_f = NULL,
};

/// Filesystem file operations.
static vfs_file_operations_t procfb_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = procfb_read,
    .write_f    = NULL,
    .lseek_f    = procfb_lseek,
    .stat_f     = NULL,
    .ioctl_f    = NULL,
    .getdents_f = NULL,
    .readlink_f = NULL,
};

int procfb_module_init(void)
{
    // Create the file.
    proc_dir_entry_t *file = proc_create_entry("feedback", NULL);
    if (file == NULL) {
        pr_err("Cannot create `/proc/feedback`.\n");
        return 1;
    }
    pr_debug("Created `/proc/feedback` (%p)\n", (void *)file);
    // Set the specific operations.
    file->sys_operations = &procfb_sys_operations;
    file->fs_operations  = &procfb_fs_operations;
    if (proc_entry_set_mask(file, 0444) < 0) {
        pr_err("Cannot set mask of `/proc/feedback`.\n");
        return 1;
    }
    return 0;
}
