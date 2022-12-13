/// @file procfs.h
/// @brief Proc file system public functions and structures.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "process/process.h"

/// @brief Stores information about a procfs directory entry.
typedef struct proc_dir_entry_t {
    /// Generic system operations.
    vfs_sys_operations_t *sys_operations;
    /// Files operations.
    vfs_file_operations_t *fs_operations;
    /// Data associated with the dir_entry.
    void *data;
    /// Name of the entry.
    const char *name;
} proc_dir_entry_t;

/// @brief Initialize the procfs filesystem.
/// @return 0 if fails, 1 if succeed.
int procfs_module_init();

/// @brief Clean up the procfs filesystem.
/// @return 0 if fails, 1 if succeed.
int procfs_cleanup_module();

/// @brief Finds the direntry inside `/proc` or under the given `parent`.
/// @param name   The name of the entry we are searching.
/// @param parent The parent (optional).
/// @return A pointer to the entry, or NULL.
proc_dir_entry_t *proc_dir_entry_get(const char *name, proc_dir_entry_t *parent);

/// @brief Creates a new directory inside the procfs filesystem.
/// @param name   The name of the entry we are creating.
/// @param parent The parent (optional).
/// @return A pointer to the entry, or NULL if fails.
proc_dir_entry_t *proc_mkdir(const char *name, proc_dir_entry_t *parent);

/// @brief Removes a directory from the procfs filesystem.
/// @param name   The name of the entry we are removing.
/// @param parent The parent (optional).
/// @return 0 if succeed, or -errno in case of error.
int proc_rmdir(const char *name, proc_dir_entry_t *parent);

/// @brief Creates a new entry inside the procfs filesystem.
/// @param name   The name of the entry we are creating.
/// @param parent The parent (optional).
/// @return A pointer to the entry, or NULL if fails.
proc_dir_entry_t *proc_create_entry(const char *name, proc_dir_entry_t *parent);

/// @brief Removes an entry from the procfs filesystem.
/// @param name   The name of the entry we are removing.
/// @param parent The parent (optional).
/// @return 0 if succeed, or -errno in case of error.
int proc_destroy_entry(const char *name, proc_dir_entry_t *parent);

/// @brief Create the entire procfs entry tree for the give process.
/// @param entry Pointer to the task_struct of the process.
/// @return 0 if succeed, or -errno in case of error.
int procr_create_entry_pid(task_struct *entry);

/// @brief Destroy the entire procfs entry tree for the give process.
/// @param entry Pointer to the task_struct of the process.
/// @return 0 if succeed, or -errno in case of error.
int procr_destroy_entry_pid(task_struct *entry);
