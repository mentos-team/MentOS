/// @file proc_feedback.c
/// @brief Contains callbacks for procfs system files.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "errno.h"
#include "fs/procfs.h"
#include "io/debug.h"
#include "process/process.h"
#include "string.h"

/// @brief Reads data from the /proc/feedback file.
///
/// @param file A pointer to the vfs_file_t structure representing the file to read from.
/// @param buf A buffer to store the read data (unused in this example).
/// @param offset The offset from where the read operation should begin (unused in this example).
/// @param nbyte The number of bytes to read (unused in this example).
/// @return Always returns 0 on success, or -ENOENT if the file is NULL.
static ssize_t procfb_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    // Check if the file pointer is NULL.
    if (!file) {
        pr_err("procfb_read: Received a NULL file.\n");
        return -ENOENT; // Return an error if the file is NULL.
    }

    // Check if the file name matches "/proc/feedback".
    if (!strcmp(file->name, "/proc/feedback")) {
        pr_alert("procfb_read: Returning scheduling feedback information.\n");
        // TODO: Add logic to return actual feedback information here.
    }

    // Return 0 to indicate success.
    return 0;
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
    .lseek_f    = NULL,
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
    pr_debug("Created `/proc/feedback` (%p)\n", file);
    // Set the specific operations.
    file->sys_operations = &procfb_sys_operations;
    file->fs_operations  = &procfb_fs_operations;
    if (proc_entry_set_mask(file, 0444) < 0) {
        pr_err("Cannot set mask of `/proc/feedback`.\n");
        return 1;
    }
    return 0;
}
