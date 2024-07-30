/// @file proc_ipc.c
/// @brief Contains callbacks for procfs system files.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "process/process.h"
#include "fs/procfs.h"
#include "io/debug.h"
#include "string.h"
#include "sys/errno.h"
#include "sys/msg.h"
#include "sys/sem.h"
#include "sys/shm.h"

extern ssize_t procipc_msg_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte);

extern ssize_t procipc_sem_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte);

extern ssize_t procipc_shm_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte);

/// Filesystem general operations.
static vfs_sys_operations_t procipc_sys_operations = {
    .mkdir_f   = NULL,
    .rmdir_f   = NULL,
    .stat_f    = NULL,
    .creat_f   = NULL,
    .symlink_f = NULL,
};

/// Filesystem file operations for message queues.
static vfs_file_operations_t procipc_msg_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = procipc_msg_read,
    .write_f    = NULL,
    .lseek_f    = NULL,
    .stat_f     = NULL,
    .ioctl_f    = NULL,
    .getdents_f = NULL,
    .readlink_f = NULL,
};

/// Filesystem file operations for semaphores.
static vfs_file_operations_t procipc_sem_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = procipc_sem_read,
    .write_f    = NULL,
    .lseek_f    = NULL,
    .stat_f     = NULL,
    .ioctl_f    = NULL,
    .getdents_f = NULL,
    .readlink_f = NULL,
};

/// Filesystem file operations for shared memry.
static vfs_file_operations_t procipc_shm_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = procipc_shm_read,
    .write_f    = NULL,
    .lseek_f    = NULL,
    .stat_f     = NULL,
    .ioctl_f    = NULL,
    .getdents_f = NULL,
    .readlink_f = NULL,
};

int procipc_module_init(void)
{
    int err = 0;
    proc_dir_entry_t *folder = NULL, *entry = NULL;

    // First, we need to create the `/proc/ipc` folder.
    if ((folder = proc_mkdir("ipc", NULL)) == NULL) {
        pr_err("Cannot create the `/proc/ipc` directory.\n");
        return 1;
    }

    if ((err = proc_entry_set_mask(folder, 0555))) {
        pr_err("Cannot set mask of `/proc/ipc` directory.\n");
        return err;
    }

    char *entry_names[] = {"msg", "sem", "shm"};
    for (int i = 0; i < count_of(entry_names); i++) {
        char *entry_name = entry_names[i];
        // Create the `/proc/ipc/` entry.
        if ((entry = proc_create_entry(entry_name, folder)) == NULL) {
            pr_err("Cannot create the `/proc/ipc/%s` file.\n", entry_name);
            return 1;
        }
        // Set the specific operations.
        entry->sys_operations = &procipc_sys_operations;
        entry->fs_operations  = &procipc_msg_fs_operations;

        if ((err = proc_entry_set_mask(entry, 0444))) {
            pr_err("Cannot set mask of `/proc/ipc/%s` file.\n", entry_name);
            return err;
        }
    }
    return 0;
}
