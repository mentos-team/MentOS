/// @file proc_running.c
/// @brief Implementaiton of procr filesystem.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fs/procfs.h"

#include "errno.h"
#include "io/debug.h"
#include "libgen.h"
#include "process/prio.h"
#include "process/process.h"
#include "stdio.h"
#include "string.h"

/// A stat record contains a NAME_MAX task name plus 52 fields.  Two standard
/// formatting buffers leave room for the complete record without consuming a
/// page-sized temporary on the per-task kernel stack.
#define PROC_RECORD_BUFFER_SIZE (2 * BUFSIZ)

/// @brief Returns the character identifying the process state.
/// @param state the process state.
/// @return a character describing the state.
/// @details
///     R  Running
///     S  Sleeping in an interruptible wait
///     D  Waiting in uninterruptible disk sleep
///     Z  Zombie
///     T  Stopped
///     t  Tracing stop
///     X  Dead
static inline char __procr_get_task_state_char(int state)
{
    if (state == 0x00) {
        return 'R';
    } // TASK_RUNNING
    if (state == (1 << 0)) {
        return 'S';
    } // TASK_INTERRUPTIBLE
    if (state == (1 << 1)) {
        return 'D';
    } // TASK_UNINTERRUPTIBLE
    if (state == (1 << 2)) {
        return 'T';
    } // TASK_STOPPED
    if (state == (1 << 3)) {
        return 't';
    } // TASK_TRACED
    if (state == (1 << 4)) {
        return 'Z';
    } // EXIT_ZOMBIE
    if (state == (1 << 5)) {
        return 'X';
    } // EXIT_DEAD
    return '?';
}

/// @brief Returns the data for the `/proc/<PID>/cmdline` file.
/// @param buffer the buffer where the data should be placed.
/// @param bufsize the size of the buffer.
/// @param task the task associated with the `/proc/<PID>` folder.
/// @return size of the written data in buffer.
static inline ssize_t __procr_do_cmdline(char *buffer, size_t bufsize, task_struct *task)
{
    int written = snprintf(buffer, bufsize, "%s", task->name);
    return (written < 0 || (size_t)written >= bufsize) ? -EOVERFLOW : written;
}

/// @brief Returns the data for the `/proc/<PID>/stat` file.
/// @param buffer the buffer where the data should be placed.
/// @param bufsize the size of the buffer.
/// @param task the task associated with the `/proc/<PID>` folder.
/// @return size of the written data in buffer, or `-EOVERFLOW` when the
///         record does not fit.
static inline ssize_t __procr_do_stat_bounded(char *buffer, size_t bufsize, task_struct *task)
{
    int written = snprintf(
        buffer, bufsize,
        "%d (%s) %c %d 0 0 0 0 0 0 0 0 0 0 0 0 0 %d %d 0 0 %u %u 0 0 %u %u %u %u %u %u %u %u %u %u %u %u %u %d\n",
        task->pid, basename(task->name), __procr_get_task_state_char(task->state), task->parent ? task->parent->pid : 0,
        task->se.prio, PRIO_TO_NICE(task->se.prio), task->se.exec_start, task->mm->total_vm, task->mm->start_code,
        task->mm->end_code, task->mm->start_stack, task->thread.regs.useresp, task->thread.regs.eip,
        task->se.prio < 100 ? task->se.prio : 0, task->mm->start_data, task->mm->end_data, task->mm->start_brk,
        task->mm->arg_start, task->mm->arg_end, task->mm->env_start, task->mm->env_end, task->exit_code);
    return (written < 0 || (size_t)written >= bufsize) ? -EOVERFLOW : written;
}

/// @brief Performs a read of files inside the `/proc/<PID>/` folder.
/// @param file is the `/proc/<PID>/` folder, thus, it should be a `proc_dir_entry_t` data.
/// @param buffer buffer where the read content must be placed.
/// @param offset offset from which we start reading from the file.
/// @param nbyte the number of bytes to read.
/// @return The number of bytes we read.
static inline ssize_t __procr_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte)
{
    if (file == NULL) {
        return -EFAULT;
    }
    // Get the entry.
    proc_dir_entry_t *entry = (proc_dir_entry_t *)file->device;
    if (entry == NULL) {
        return -EFAULT;
    }
    // Get the task.
    task_struct *task = (task_struct *)entry->data;
    if (task == NULL) {
        return -EFAULT;
    }
    // Prepare a support buffer.
    char support[PROC_RECORD_BUFFER_SIZE];
    memset(support, 0, sizeof(support));
    // Call the specific function.
    if (strcmp(entry->name, "cmdline") == 0) {
        if (__procr_do_cmdline(support, sizeof(support), task) < 0) {
            return -EOVERFLOW;
        }
    } else if (strcmp(entry->name, "stat") == 0) {
        if (__procr_do_stat_bounded(support, sizeof(support), task) < 0) {
            return -EOVERFLOW;
        }
    }
    // Copmute the amounts of bytes we want (and can) read.
    ssize_t bytes_to_read = max(0, min(strlen(support) - offset, nbyte));
    // Perform the read: copy exactly the computed amount, never more, since
    // buffer is the raw user read(2) buffer and nbyte is all it can hold
    // (#194: a strcpy here used to write the whole file through it).
    if (bytes_to_read > 0) {
        memcpy(buffer, support + offset, (size_t)bytes_to_read);
    }
    return bytes_to_read;
}

/// Filesystem general operations.
static vfs_sys_operations_t procr_sys_operations = {
    .mkdir_f   = NULL,
    .rmdir_f   = NULL,
    .stat_f    = NULL,
    .creat_f   = NULL,
    .symlink_f = NULL,
};

/// Filesystem file operations.
static vfs_file_operations_t procr_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = __procr_read,
    .write_f    = NULL,
    .lseek_f    = NULL,
    .stat_f     = NULL,
    .ioctl_f    = NULL,
    .getdents_f = NULL,
    .readlink_f = NULL,
};

int procr_create_entry_pid(task_struct *entry)
{
    char path[PATH_MAX];
    proc_dir_entry_t *proc_dir   = NULL;
    proc_dir_entry_t *proc_entry = NULL;
    {
        // Create `/proc/[PID]`.
        sprintf(path, "%d", entry->pid);
        // Create the proc entry root directory.
        if ((proc_dir = proc_mkdir(path, NULL)) == NULL) {
            pr_err("[task: %d] Cannot create proc root directory `%s`.\n", entry->pid, path);
            return -ENOENT;
        }
        proc_dir->data = entry;
    }
    {
        // Create `/proc/[PID]/cmdline`.
        if ((proc_entry = proc_create_entry("cmdline", proc_dir)) == NULL) {
            pr_err("[task: %d] Cannot create proc entry `%s`.\n", entry->pid, path);
            return -ENOENT;
        }
        proc_entry->sys_operations = &procr_sys_operations;
        proc_entry->fs_operations  = &procr_fs_operations;
        proc_entry->data           = entry;
    }
    {
        // Create `/proc/[PID]/stat`.
        if ((proc_entry = proc_create_entry("stat", proc_dir)) == NULL) {
            pr_err("[task: %d] Cannot create proc entry `%s`.\n", entry->pid, path);
            return -ENOENT;
        }
        proc_entry->sys_operations = &procr_sys_operations;
        proc_entry->fs_operations  = &procr_fs_operations;
        proc_entry->data           = entry;
    }
    return 0;
}

int procr_destroy_entry_pid(task_struct *entry)
{
    // Turn the pid into string. The maximum pid is 32768, thus entry pid is at most 6 chars.
    char pid_str[6];
    sprintf(pid_str, "%d", entry->pid);
    // Get the root directory.
    proc_dir_entry_t *proc_dir = proc_dir_entry_get(pid_str, NULL);
    if (proc_dir == NULL) {
        pr_err("[task: %d] Cannot find proc root directory `%s`.\n", entry->pid, pid_str);
        return -ENOENT;
    }
    // Destroy `/proc/[PID]/cmdline`.
    if (proc_destroy_entry("cmdline", proc_dir)) {
        pr_err("[task: %d] Cannot destroy proc cmdline.\n", entry->pid);
        return -ENOENT;
    }
    // Destroy `/proc/[PID]/stat`.
    if (proc_destroy_entry("stat", proc_dir)) {
        pr_err("[task: %d] Cannot destroy proc stat.\n", entry->pid);
        return -ENOENT;
    }
    // Destroy `/proc/[PID]`.
    if (proc_rmdir(pid_str, NULL)) {
        pr_err("[task: %d] Cannot remove proc root directory `%s`.\n", entry->pid, pid_str);
        return -ENOENT;
    }
    return 0;
}
