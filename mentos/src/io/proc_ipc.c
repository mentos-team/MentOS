/// @file proc_ipc.c
/// @brief Contains callbacks for procfs system files.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fs/procfs.h"
#include "process/process.h"
#include "sys/errno.h"
#include "io/debug.h"
#include "string.h"

static ssize_t procipc_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    if (!file) {
        pr_err("Received a NULL file.\n");
        return -ENOENT;
    }
    if (!strcmp(file->name, "/proc/ipc/msg")) {
        pr_alert("Return MSG stat.\n");
    }
    if (!strcmp(file->name, "/proc/ipc/sem")) {
        pr_alert("Return SEM stat.\n");
    }
    if (!strcmp(file->name, "/proc/ipc/shm")) {
        pr_alert("Return SHM stat.\n");
    }
    return 0;
}

/// Filesystem general operations.
static vfs_sys_operations_t procipc_sys_operations = {
    .mkdir_f = NULL,
    .rmdir_f = NULL,
    .stat_f  = NULL
};

/// Filesystem file operations.
static vfs_file_operations_t procipc_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = procipc_read,
    .write_f    = NULL,
    .lseek_f    = NULL,
    .stat_f     = NULL,
    .ioctl_f    = NULL,
    .getdents_f = NULL
};

int procipc_module_init()
{
    proc_dir_entry_t *folder = NULL, *entry = NULL;
    // First, we need to create the `/proc/ipc` folder.
    if ((folder = proc_mkdir("ipc", NULL)) == NULL) {
        pr_err("Cannot create the `/proc/ipc` directory.\n");
        return 1;
    }
    // Create the `/proc/ipc/msg` entry.
    if ((entry = proc_create_entry("msg", folder)) == NULL) {
        pr_err("Cannot create the `/proc/ipc/msg` file.\n");
        return 1;
    }
    // Set the specific operations.
    entry->sys_operations = &procipc_sys_operations;
    entry->fs_operations  = &procipc_fs_operations;
    // Create the `/proc/ipc/sem` entry.
    if ((entry = proc_create_entry("sem", folder)) == NULL) {
        pr_err("Cannot create the `/proc/ipc/sem` file.\n");
        return 1;
    }
    // Set the specific operations.
    entry->sys_operations = &procipc_sys_operations;
    entry->fs_operations  = &procipc_fs_operations;
    // Create the `/proc/ipc/shm` entry.
    if ((entry = proc_create_entry("shm", folder)) == NULL) {
        pr_err("Cannot create the `/proc/ipc/shm` file.\n");
        return 1;
    }
    // Set the specific operations.
    entry->sys_operations = &procipc_sys_operations;
    entry->fs_operations  = &procipc_fs_operations;
    return 0;
}
