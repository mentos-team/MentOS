/// @file proc_feedback.c
/// @brief Contains callbacks for procfs system files.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fs/procfs.h"
#include "process/process.h"
#include "sys/errno.h"
#include "io/debug.h"
#include "string.h"

static ssize_t procfb_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    if (!file) {
        pr_err("Received a NULL file.\n");
        return -ENOENT;
    }
    if (!strcmp(file->name, "/proc/feedback")) {
        pr_alert("Return scheduling feedback information.\n");
    }
    return 0;
}

/// Filesystem general operations.
static vfs_sys_operations_t procfb_sys_operations = {
    .mkdir_f = NULL,
    .rmdir_f = NULL,
    .stat_f  = NULL
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
    .getdents_f = NULL
};

int procfb_module_init()
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
    return 0;
}
