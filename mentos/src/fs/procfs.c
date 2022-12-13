/// @file procfs.c
/// @brief Proc file system implementation.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[PROCFS]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "fs/procfs.h"
#include "fs/vfs.h"
#include "string.h"
#include "sys/errno.h"
#include "io/debug.h"
#include "fcntl.h"
#include "libgen.h"
#include "assert.h"
#include "stdio.h"
#include "time.h"

/// Maximum length of name in PROCFS.
#define PROCFS_NAME_MAX 255U
/// Maximum number of files in PROCFS.
#define PROCFS_MAX_FILES 1024U
/// The magic number used to check if the procfs file is valid.
#define PROCFS_MAGIC_NUMBER 0xBF

// ============================================================================
// Data Structures
// ============================================================================

/// @brief Information concerning a file.
typedef struct procfs_file_t {
    /// Number used as delimiter, it must be set to 0xBF.
    int magic;
    /// The file inode.
    int inode;
    /// Flags.
    unsigned flags;
    /// The file mask.
    mode_t mask;
    /// The name of the file.
    char name[PROCFS_NAME_MAX];
    /// User id of the file.
    uid_t uid;
    /// Group id of the file.
    gid_t gid;
    /// Time of last access.
    time_t atime;
    /// Time of last data modification.
    time_t mtime;
    /// Time of last status change.
    time_t ctime;
    /// Pointer to the associated proc_dir_entry_t.
    proc_dir_entry_t dir_entry;
    /// Associated files.
    list_head files;
    /// List of procfs siblings.
    list_head siblings;
} procfs_file_t;

/// @brief The details regarding the filesystem.
/// @brief Contains the number of files inside the procfs filesystem.
typedef struct procfs_t {
    /// Number of files.
    unsigned int nfiles;
    /// List of headers.
    list_head files;
    /// Cache for creating new `procfs_file_t`.
    kmem_cache_t *procfs_file_cache;
} procfs_t;

/// The procfs filesystem.
procfs_t fs;

// ============================================================================
// Forward Declaration of Functions
// ============================================================================

static int procfs_mkdir(const char *path, mode_t mode);
static int procfs_rmdir(const char *path);
static int procfs_stat(const char *path, stat_t *stat);

static vfs_file_t *procfs_open(const char *path, int flags, mode_t mode);
static int procfs_unlink(const char *path);
static int procfs_close(vfs_file_t *file);
static ssize_t procfs_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte);
static ssize_t procfs_write(vfs_file_t *file, const void *buffer, off_t offset, size_t nbyte);
static off_t procfs_lseek(vfs_file_t *file, off_t offset, int whence);
static int procfs_fstat(vfs_file_t *file, stat_t *stat);
static int procfs_ioctl(vfs_file_t *file, int request, void *data);
static int procfs_getdents(vfs_file_t *file, dirent_t *dirp, off_t doff, size_t count);

// ============================================================================
// Virtual FileSystem (VFS) Operaions
// ============================================================================

/// Filesystem general operations.
static vfs_sys_operations_t procfs_sys_operations = {
    .mkdir_f = procfs_mkdir,
    .rmdir_f = procfs_rmdir,
    .stat_f  = procfs_stat
};

/// Filesystem file operations.
static vfs_file_operations_t procfs_fs_operations = {
    .open_f     = procfs_open,
    .unlink_f   = procfs_unlink,
    .close_f    = procfs_close,
    .read_f     = procfs_read,
    .write_f    = procfs_write,
    .lseek_f    = procfs_lseek,
    .stat_f     = procfs_fstat,
    .ioctl_f    = procfs_ioctl,
    .getdents_f = procfs_getdents
};

// ============================================================================
// PROCFS Core Functions
// ============================================================================

/// @brief Checks if the file is a valid PROCFS file.
/// @param procfs_file the file to check.
/// @return true if valid, false otherwise.
static inline bool_t procfs_check_file(procfs_file_t *procfs_file)
{
    return (procfs_file && (procfs_file->magic == PROCFS_MAGIC_NUMBER));
}

/// @brief Returns the PROCFS file associated with the given list entry.
/// @param entry the entry to transform to PROCFS file.
/// @return a valid pointer to a PROCFS file, NULL otherwise.
static inline procfs_file_t *procfs_get_file(list_head *entry)
{
    procfs_file_t *procfs_file;
    if (entry)
        if (procfs_check_file((procfs_file = list_entry(entry, procfs_file_t, siblings))))
            return procfs_file;
    return NULL;
}

/// @brief Finds the PROCFS file at the given path.
/// @param path the path to the entry.
/// @return a pointer to the PROCFS file, NULL otherwise.
static inline procfs_file_t *procfs_find_entry_path(const char *path)
{
    procfs_file_t *procfs_file;
    if (!list_head_empty(&fs.files)) {
        list_for_each_decl(it, &fs.files)
        {
            // Get the file structure.
            procfs_file = procfs_get_file(it);
            // Check its name.
            if (procfs_file && !strcmp(procfs_file->name, path))
                return procfs_file;
        }
    }
    return NULL;
}

/// @brief Finds the PROCFS file with the given inode.
/// @param inode the inode we search.
/// @return a pointer to the PROCFS file, NULL otherwise.
static inline procfs_file_t *procfs_find_entry_inode(int inode)
{
    procfs_file_t *procfs_file;
    if (!list_head_empty(&fs.files)) {
        list_for_each_decl(it, &fs.files)
        {
            // Get the file structure.
            procfs_file = procfs_get_file(it);
            // Check its inode.
            if (procfs_file && (procfs_file->inode == inode)) {
                return procfs_file;
            }
        }
    }
    return NULL;
}

/// @brief Finds the inode associated with a PROCFS file at the given path.
/// @param path the path to the entry.
/// @return a valid inode, or -1 on failure.
static inline int procfs_find_inode(const char *path)
{
    procfs_file_t *procfs_file = procfs_find_entry_path(path);
    if (procfs_file)
        return procfs_file->inode;
    return -1;
}

static inline int procfs_get_free_inode()
{
    for (int inode = 1; inode < PROCFS_MAX_FILES; ++inode)
        if (procfs_find_entry_inode(inode) == NULL)
            return inode;
    return -1;
}

/// @brief Checks if the PROCFS directory at the given path is empty.
/// @param path the path to the directory.
/// @return 0 if empty, 1 if not.
static inline int procfs_check_if_empty(const char *path)
{
    procfs_file_t *procfs_file;
    if (!list_head_empty(&fs.files)) {
        list_for_each_decl(it, &fs.files)
        {
            // Get the file structure.
            procfs_file = procfs_get_file(it);
            // Check if it a valid pointer.
            if (procfs_file) {
                // It's the directory itself.
                if (!strcmp(path, procfs_file->name))
                    continue;
                // Get the directory of the file.
                char *filedir = dirname(procfs_file->name);
                // Check if directory path and file directory are the same.
                if (filedir && !strcmp(path, filedir))
                    return 1;
            }
        }
    }
    return 0;
}

/// @brief Creates a new PROCFS file.
/// @param path where the file resides.
/// @param flags the creation flags.
/// @return a pointer to the new PROCFS file, NULL otherwise.
static inline procfs_file_t *procfs_create_file(const char *path, unsigned flags)
{
    procfs_file_t *procfs_file = (procfs_file_t *)kmem_cache_alloc(fs.procfs_file_cache, GFP_KERNEL);
    if (!procfs_file) {
        pr_err("Failed to get free entry (%p).\n", procfs_file);
        return NULL;
    }
    // Clean up the memory.
    memset(procfs_file, 0, sizeof(procfs_file_t));
    // Initialize the magic number.
    procfs_file->magic = PROCFS_MAGIC_NUMBER;
    // Initialize the inode.
    procfs_file->inode = procfs_get_free_inode();
    // Flags.
    procfs_file->flags = flags;
    // The name of the file.
    strcpy(procfs_file->name, path);
    // Associated files.
    list_head_init(&procfs_file->files);
    // List of all the PROCFS files.
    list_head_init(&procfs_file->siblings);
    // Add the file to the list of opened files.
    list_head_insert_before(&procfs_file->siblings, &fs.files);
    // Time of last access.
    procfs_file->atime = sys_time(NULL);
    // Time of last data modification.
    procfs_file->mtime = procfs_file->atime;
    // Time of last status change.
    procfs_file->ctime = procfs_file->atime;
    // Initialize the dir_entry.
    procfs_file->dir_entry.name           = basename(procfs_file->name);
    procfs_file->dir_entry.data           = NULL;
    procfs_file->dir_entry.sys_operations = NULL;
    procfs_file->dir_entry.fs_operations  = NULL;
    // Increase the number of files.
    ++fs.nfiles;
    pr_debug("procfs_create_file(%p) `%s`\n", procfs_file, path);
    return procfs_file;
}

/// @brief Destroyes the given PROCFS file.
/// @param procfs_file pointer to the PROCFS file to destroy.
/// @return 0 on success, 1 on failure.
static inline int procfs_destroy_file(procfs_file_t *procfs_file)
{
    if (!procfs_file) {
        pr_err("Received a null entry (%p).\n", procfs_file);
        return 1;
    }
    pr_debug("procfs_destroy_file(%p) `%s`\n", procfs_file, procfs_file->name);
    // Remove the file from the list of opened files.
    list_head_remove(&procfs_file->siblings);
    // Free the cache.
    kmem_cache_free(procfs_file);
    // Decrease the number of files.
    --fs.nfiles;
    return 0;
}

/// @brief Creates a VFS file, from a PROCFS file.
/// @param procfs_file the PROCFS file.
/// @return a pointer to the newly create VFS file, NULL on failure.
static inline vfs_file_t *procfs_create_file_struct(procfs_file_t *procfs_file)
{
    if (!procfs_file) {
        pr_err("procfs_create_file_struct(%p): Procfs file not valid!\n", procfs_file);
        return NULL;
    }
    vfs_file_t *vfs_file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
    if (!vfs_file) {
        pr_err("procfs_create_file_struct(%p): Failed to allocate memory for VFS file!\n", procfs_file);
        return NULL;
    }
    memset(vfs_file, 0, sizeof(vfs_file_t));
    strcpy(vfs_file->name, procfs_file->name);
    vfs_file->device         = &procfs_file->dir_entry;
    vfs_file->ino            = procfs_file->inode;
    vfs_file->uid            = 0;
    vfs_file->gid            = 0;
    vfs_file->mask           = S_IRUSR | S_IRGRP | S_IROTH;
    vfs_file->length         = 0;
    vfs_file->flags          = procfs_file->flags;
    vfs_file->sys_operations = &procfs_sys_operations;
    vfs_file->fs_operations  = &procfs_fs_operations;
    list_head_init(&vfs_file->siblings);
    pr_debug("procfs_create_file_struct(%p): VFS file : %p\n", procfs_file, vfs_file);
    return vfs_file;
}

/// @brief Dumps on debugging output the PROCFS.
static void dump_procfs()
{
    procfs_file_t *file;
    if (!list_head_empty(&fs.files)) {
        list_for_each_decl(it, &fs.files)
        {
            // Get the file structure.
            file = procfs_get_file(it);
            // Check if it a valid procfs file.
            if (file)
                pr_debug("[%3d] `%s`\n", file->inode, file->name);
        }
    }
}

// ============================================================================
// Virtual FileSystem (VFS) Functions
// ============================================================================

/// @brief Creates a new directory.
/// @param path The path to the new directory.
/// @param mode The file mode.
/// @return 0   if success.
static int procfs_mkdir(const char *path, mode_t mode)
{
    if ((strcmp(path, ".") == 0) || (strcmp(path, "..") == 0)) {
        pr_err("procfs_mkdir(%s): Cannot create `.` or `..`.\n", path);
        return -EPERM;
    }
    if (procfs_find_entry_path(path) != NULL) {
        return -EEXIST;
    }
    // Check if the directories before it exist.
    procfs_file_t *parent_file = NULL;
    char *parent               = dirname(path);
    if ((strcmp(parent, ".") != 0) && (strcmp(parent, "/") != 0)) {
        parent_file = procfs_find_entry_path(parent);
        if (parent_file == NULL) {
            return -ENOENT;
        }
        if ((parent_file->flags & DT_DIR) == 0) {
            return -ENOTDIR;
        }
    }
    // Create the new procfs file.
    procfs_file_t *procfs_file = procfs_create_file(path, DT_DIR);
    if (!procfs_file) {
        pr_err("procfs_mkdir(%s): Cannot create the procfs file.\n", path);
        return -ENFILE;
    }
    return 0;
}

/// @brief Removes a directory.
/// @param path The path to the directory.
/// @return 0 if success.
static int procfs_rmdir(const char *path)
{
    if ((strcmp(path, ".") == 0) || (strcmp(path, "..") == 0)) {
        pr_err("procfs_rmdir(%s): Cannot remove `.` or `..`.\n", path);
        return -EPERM;
    }
    // Get the file.
    procfs_file_t *procfs_file = procfs_find_entry_path(path);
    if (procfs_file == NULL) {
        pr_err("procfs_rmdir(%s): Cannot find the file.\n", path);
        return -ENOENT;
    }
    // Check the type.
    if ((procfs_file->flags & DT_DIR) == 0) {
        pr_err("procfs_rmdir(%s): The entry is not a directory.\n", path);
        return -ENOTDIR;
    }
    // Check if the directory is currently opened.
    if (!list_head_empty(&procfs_file->files)) {
        pr_err("procfs_rmdir(%s): The directory is opened by someone.\n", path);
        return -EBUSY;
    }
    // Check if its empty.
    if (procfs_check_if_empty(procfs_file->name)) {
        pr_err("procfs_rmdir(%s): The directory is not empty.\n", path);
        return -ENOTEMPTY;
    }
    if (procfs_destroy_file(procfs_file)) {
        pr_err("procfs_rmdir(%s): Failed to remove directory.\n", path);
    }
    return 0;
}

/// @brief Open the file at the given path and returns its file descriptor.
/// @param path  The path to the file.
/// @param flags The flags used to determine the behavior of the function.
/// @param mode  The mode with which we open the file.
/// @return The file descriptor of the opened file, otherwise returns -1.
static vfs_file_t *procfs_open(const char *path, int flags, mode_t mode)
{
    // Get the parent path.
    char *parent_path = dirname(path);
    // Check if the directories before it exist.
    if ((strcmp(parent_path, ".") != 0) && (strcmp(parent_path, "/") != 0)) {
        procfs_file_t *parent_file = procfs_find_entry_path(parent_path);
        if (parent_file == NULL) {
            pr_err("Cannot find parent `%s`.\n", parent_path);
            errno = ENOENT;
            return NULL;
        }
        if (!bitmask_check(parent_file->flags, DT_DIR)) {
            pr_err("Parent folder `%s` is not a directory.\n", parent_path);
            errno = ENOTDIR;
            return NULL;
        }
    }
    // Find the entry.
    procfs_file_t *procfs_file = procfs_find_entry_path(path);
    if (procfs_file != NULL) {
        // Check if the user wants to create a file.
        if (bitmask_check(flags, O_CREAT | O_EXCL)) {
            pr_err("Cannot create, it exists `%s`.\n", path);
            errno = EEXIST;
            return NULL;
        }
        // Check if the user wants to open a directory.
        if (bitmask_check(flags, O_DIRECTORY)) {
            // Check if the file is a directory.
            if (!bitmask_check(procfs_file->flags, DT_DIR)) {
                pr_err("Is not a directory `%s` but access requested involved a directory.\n", path);
                errno = ENOTDIR;
                return NULL;
            }
            // Check if pathname refers to a directory and the access requested
            // involved writing.
            if (bitmask_check(flags, O_RDWR) || bitmask_check(flags, O_WRONLY)) {
                pr_err("Is a directory `%s` but access requested involved writing.\n", path);
                errno = EISDIR;
                return NULL;
            }
            // Create the associated file.
            vfs_file_t *vfs_file = procfs_create_file_struct(procfs_file);
            if (!vfs_file) {
                pr_err("Cannot create vfs file for opening directory `%s`.\n", path);
                errno = ENFILE;
                return NULL;
            }
            // Update file access.
            procfs_file->atime = sys_time(NULL);
            // Add the vfs_file to the list of associated files.
            list_head_insert_before(&vfs_file->siblings, &procfs_file->files);
            return vfs_file;
        }
        // Check if the user did not want to open a directory, but it is.
        if (bitmask_check(procfs_file->flags, DT_DIR)) {
            pr_err("Is a directory `%s` but access requested did not involved a directory.\n", path);
            errno = EISDIR;
            return NULL;
        }
        // Create the associated file.
        vfs_file_t *vfs_file = procfs_create_file_struct(procfs_file);
        if (!vfs_file) {
            pr_err("Cannot create vfs file for opening file `%s`...\n", path);
            errno = ENFILE;
            return NULL;
        }
        // Update file access.
        procfs_file->atime = sys_time(NULL);
        // Add the vfs_file to the list of associated files.
        list_head_insert_before(&vfs_file->siblings, &procfs_file->files);
        return vfs_file;
    }
    //  When both O_CREAT and O_DIRECTORY are specified in flags and the file
    //  specified by pathname does not exist, open() will create a regular file
    //  (i.e., O_DIRECTORY is ignored).
    if (bitmask_check(flags, O_CREAT)) {
        // Create the new procfs file.
        procfs_file = procfs_create_file(path, DT_REG);
        if (!procfs_file) {
            pr_err("Cannot create procfs_file for `%s`...\n", path);
            errno = ENFILE;
            return NULL;
        }
        // Create the associated file.
        vfs_file_t *vfs_file = procfs_create_file_struct(procfs_file);
        if (!vfs_file) {
            pr_err("Cannot create vfs file for opening file `%s`...\n", path);
            errno = ENFILE;
            return NULL;
        }
        // Add the vfs_file to the list of associated files.
        list_head_insert_before(&vfs_file->siblings, &procfs_file->files);
        pr_debug("Created file `%s`.\n", path);
        return vfs_file;
    }
    errno = ENOENT;
    return NULL;
}

/// @brief Closes the given file.
/// @param file The file structure.
static int procfs_close(vfs_file_t *file)
{
    assert(file && "Received null file.");
    //pr_debug("procfs_close(%p): VFS file : %p\n", file, file);
    // Remove the file from the list of `files` inside its corresponding `procfs_file_t`.
    list_head_remove(&file->siblings);
    // Free the memory of the file.
    kmem_cache_free(file);
    return 0;
}

/// @brief Deletes the file at the given path.
/// @param path The path to the file.
/// @return On success, zero is returned. On error, -1 is returned.
static inline int procfs_unlink(const char *path)
{
    if ((strcmp(path, ".") == 0) || (strcmp(path, "..") == 0)) {
        return -EPERM;
    }
    procfs_file_t *procfs_file = procfs_find_entry_path(path);
    if (procfs_file != NULL) {
        return -EEXIST;
    }
    // Check the type.
    if ((procfs_file->flags & DT_REG) == 0) {
        if ((procfs_file->flags & DT_DIR) != 0) {
            pr_err("procfs_unlink(%s): The file is a directory.\n", path);
            return -EISDIR;
        }
        pr_err("procfs_unlink(%s): The file is not a regular file.\n", path);
        return -EACCES;
    }
    // Check if the procfs file has still some file associated.
    if (!list_head_empty(&procfs_file->files)) {
        pr_err("procfs_unlink(%s): The file is opened by someone.\n", path);
        return -EACCES;
    }
    if (procfs_destroy_file(procfs_file)) {
        pr_err("procfs_unlink(%s): Failed to remove file.\n", path);
    }
    return 0;
}

/// @brief Reads from the file identified by the file descriptor.
/// @param file The file.
/// @param buffer Buffer where the read content must be placed.
/// @param offset Offset from which we start reading from the file.
/// @param nbyte The number of bytes to read.
/// @return The number of red bytes.
static ssize_t procfs_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    if (file) {
        procfs_file_t *procfs_file = procfs_find_entry_inode(file->ino);
        if (procfs_file && procfs_file->dir_entry.fs_operations)
            if (procfs_file->dir_entry.fs_operations->read_f)
                return procfs_file->dir_entry.fs_operations->read_f(file, buf, offset, nbyte);
    }
    return -ENOSYS;
}

/// @brief Writes the given content inside the file.
/// @param file The file descriptor of the file.
/// @param buffer The content to write.
/// @param offset Offset from which we start writing in the file.
/// @param nbyte The number of bytes to write.
/// @return The number of written bytes.
static ssize_t procfs_write(vfs_file_t *file, const void *buf, off_t offset, size_t nbyte)
{
    if (file) {
        procfs_file_t *procfs_file = procfs_find_entry_inode(file->ino);
        if (procfs_file && procfs_file->dir_entry.fs_operations)
            if (procfs_file->dir_entry.fs_operations->write_f)
                return procfs_file->dir_entry.fs_operations->write_f(file, buf, offset, nbyte);
    }
    return -ENOSYS;
}

/// @brief Repositions the file offset inside a file.
/// @param file the file we are working with.
/// @param offset the offest to use for the operation.
/// @param whence the type of operation.
/// @return  Upon successful completion, returns the resulting offset
/// location as measured in bytes from the beginning of the file. On
/// error, the value (off_t) -1 is returned and errno is set to
/// indicate the error.
off_t procfs_lseek(vfs_file_t *file, off_t offset, int whence)
{
    if (file == NULL) {
        pr_err("Received a NULL file.\n");
        return -ENOSYS;
    }
    procfs_file_t *procfs_file = procfs_find_entry_inode(file->ino);
    if (procfs_file == NULL) {
        pr_err("There is no PROCFS fiel associated with the VFS file.\n");
        return -ENOSYS;
    }
    if (procfs_file->dir_entry.fs_operations)
        if (procfs_file->dir_entry.fs_operations->lseek_f)
            return procfs_file->dir_entry.fs_operations->lseek_f(file, offset, whence);
    return -EINVAL;
}

/// @brief Saves the information concerning the file.
/// @param inode The inode containing the data.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int __procfs_stat(procfs_file_t *file, stat_t *stat)
{
    stat->st_uid   = file->uid;
    stat->st_gid   = file->gid;
    stat->st_dev   = 0;
    stat->st_ino   = file->inode;
    stat->st_mode  = file->flags;
    stat->st_size  = 0;
    stat->st_atime = file->atime;
    stat->st_mtime = file->mtime;
    stat->st_ctime = file->ctime;
    return 0;
}

/// @brief Retrieves information concerning the file at the given position.
/// @param file The file struct.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int procfs_fstat(vfs_file_t *file, stat_t *stat)
{
    if (file && stat) {
        procfs_file_t *procfs_file = procfs_find_entry_inode(file->ino);
        if (procfs_file) {
            if (procfs_file->dir_entry.fs_operations && procfs_file->dir_entry.fs_operations->stat_f)
                return procfs_file->dir_entry.fs_operations->stat_f(file, stat);
            else
                return __procfs_stat(procfs_file, stat);
        }
    }
    return -ENOSYS;
}

/// @brief Retrieves information concerning the file at the given position.
/// @param path The path to the file.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int procfs_stat(const char *path, stat_t *stat)
{
    if (path && stat) {
        procfs_file_t *procfs_file = procfs_find_entry_path(path);
        if (procfs_file) {
            if (procfs_file->dir_entry.sys_operations && procfs_file->dir_entry.sys_operations->stat_f)
                return procfs_file->dir_entry.sys_operations->stat_f(path, stat);
            else
                return __procfs_stat(procfs_file, stat);
        }
    }
    return -1;
}

static int procfs_ioctl(vfs_file_t *file, int request, void *data)
{
    if (file) {
        procfs_file_t *procfs_file = procfs_find_entry_inode(file->ino);
        if (procfs_file)
            if (procfs_file->dir_entry.fs_operations && procfs_file->dir_entry.fs_operations->ioctl_f)
                return procfs_file->dir_entry.fs_operations->ioctl_f(file, request, data);
    }
    return -1;
}

/// @brief Reads contents of the directories to a dirent buffer, updating
///        the offset and returning the number of written bytes in the buffer,
///        it assumes that all paths are well-formed.
/// @param file  The directory handler.
/// @param dirp  The buffer where the data should be written.
/// @param doff  The offset inside the buffer where the data should be written.
/// @param count The maximum length of the buffer.
/// @return The number of written bytes in the buffer.
static inline int procfs_getdents(vfs_file_t *file, dirent_t *dirp, off_t doff, size_t count)
{
    if (!file || !dirp)
        return -1;
    procfs_file_t *direntry = procfs_find_entry_inode(file->ino);
    if (direntry == NULL) {
        return -ENOENT;
    }
    if ((direntry->flags & DT_DIR) == 0)
        return -ENOTDIR;
    memset(dirp, 0, count);
    int len        = strlen(direntry->name);
    size_t written = 0;
    off_t current  = 0;
    char *parent   = NULL;
    procfs_file_t *entry;
    if (!list_head_empty(&fs.files)) {
        list_for_each_decl(it, &fs.files)
        {
            // Get the file structure.
            entry = procfs_get_file(it);
            // Check if it a valid procfs file.
            if (!entry)
                continue;

            // If the entry is the directory itself, skip.
            if (strcmp(direntry->name, entry->name) == 0) {
                continue;
            }
            // Get the parent directory.
            parent = dirname(entry->name);
            // Check if the entry is inside the directory.
            if (strcmp(direntry->name, parent) != 0) {
                continue;
            }
            // Skip if already provided.
            if (current++ < doff) {
                continue;
            }
            if (*(entry->name + len) == '/')
                ++len;
            // Write on current dirp.
            dirp->d_ino  = entry->inode;
            dirp->d_type = entry->flags;
            strcpy(dirp->d_name, entry->name + len);
            dirp->d_off    = sizeof(dirent_t);
            dirp->d_reclen = sizeof(dirent_t);
            // Increment the written counter.
            written += sizeof(dirent_t);
            // Move to next writing position.
            dirp += 1;

            if (written >= count)
                break;
        }
    }
    return written;
}

/// @brief Mounts the block device as an EXT2 filesystem.
/// @param block_device the block device formatted as EXT2.
/// @return the VFS root node of the EXT2 filesystem.
static vfs_file_t *ext2_mount(vfs_file_t *block_device, const char *path)
{
    return NULL;
}

// ============================================================================
// Initialization Functions
// ============================================================================

/// @brief Mounts the filesystem at the given path.
/// @param path the path where we want to mount a procfs.
/// @param device we expect it to be NULL.
/// @return a pointer to the root VFS file.
static vfs_file_t *procfs_mount_callback(const char *path, const char *device)
{
    pr_debug("procfs_mount_callback(%s, %s)\n", path, device);
    // Create the new procfs file.
    procfs_file_t *procfs_file = procfs_create_file(path, DT_DIR);
    assert(procfs_file && "Failed to create procfs_file.");
    // Create the associated file.
    vfs_file_t *vfs_file = procfs_create_file_struct(procfs_file);
    assert(vfs_file && "Failed to create vfs_file.");
    // Add the vfs_file to the list of associated files.
    list_head_insert_before(&vfs_file->siblings, &procfs_file->files);
    // Initialize the proc_root.
    return vfs_file;
}

/// Filesystem information.
static file_system_type procfs_file_system_type = {
    .name     = "procfs",
    .fs_flags = 0,
    .mount    = procfs_mount_callback
};

int procfs_module_init()
{
    // Initialize the procfs.
    memset(&fs, 0, sizeof(struct procfs_t));
    // Initialize the cache.
    fs.procfs_file_cache = KMEM_CREATE(procfs_file_t);
    // Initialize the list of procfs files.
    list_head_init(&fs.files);
    // Register the filesystem.
    vfs_register_filesystem(&procfs_file_system_type);
    return 0;
}

int procfs_cleanup_module()
{
    // Destroy the cache.
    kmem_cache_destroy(fs.procfs_file_cache);
    // Unregister the filesystem.
    vfs_register_filesystem(&procfs_file_system_type);
    return 0;
}

// ============================================================================
// Publically available functions
// ============================================================================

proc_dir_entry_t *proc_dir_entry_get(const char *name, proc_dir_entry_t *parent)
{
    char entry_path[PATH_MAX];
    strcpy(entry_path, "/proc/");
    if (parent) {
        strcat(entry_path, parent->name);
        strcat(entry_path, "/");
    }
    strcat(entry_path, name);
    // Get the procfs entry.
    procfs_file_t *procfs_file = procfs_find_entry_path(entry_path);
    if (procfs_file == NULL) {
        pr_err("proc_dir_entry_get(%s): Cannot find proc entry.\n", entry_path);
        return NULL;
    }
    return &procfs_file->dir_entry;
}

proc_dir_entry_t *proc_mkdir(const char *name, proc_dir_entry_t *parent)
{
    char entry_path[PATH_MAX];
    strcpy(entry_path, "/proc/");
    if (parent) {
        strcat(entry_path, parent->name);
        strcat(entry_path, "/");
    }
    strcat(entry_path, name);
    // Check if the entry exists.
    if (procfs_find_entry_path(entry_path) != NULL) {
        pr_err("proc_destroy_entry(%s): Proc entry already exists.\n", entry_path);
        errno = EEXIST;
        return NULL;
    }
    // Create the new procfs file.
    procfs_file_t *procfs_file = procfs_create_file(entry_path, DT_DIR);
    if (!procfs_file) {
        pr_err("proc_destroy_entry(%s): Cannot create proc entry.\n", entry_path);
        errno = ENFILE;
        return NULL;
    }
    return &procfs_file->dir_entry;
}

int proc_rmdir(const char *name, proc_dir_entry_t *parent)
{
    char entry_path[PATH_MAX];
    strcpy(entry_path, "/proc/");
    if (parent) {
        strcat(entry_path, parent->name);
        strcat(entry_path, "/");
    }
    strcat(entry_path, name);
    // Check if the entry exists.
    procfs_file_t *procfs_file = procfs_find_entry_path(entry_path);
    if (procfs_file == NULL) {
        pr_err("proc_destroy_entry(%s): Cannot find proc entry.\n", entry_path);
        return -ENOENT;
    }
    if ((procfs_file->flags & DT_DIR) == 0) {
        pr_err("proc_destroy_entry(%s): Proc entry is not a directory.\n", entry_path);
        return -ENOTDIR;
    }
    // Check if its empty.
    if (procfs_check_if_empty(procfs_file->name)) {
        pr_err("procfs_rmdir(%s): The directory is not empty.\n", entry_path);
        return -ENOTEMPTY;
    }
    // Check if the procfs file has still some file associated.
    if (!list_head_empty(&procfs_file->files)) {
        pr_err("proc_destroy_entry(%s): Proc entry is busy.\n", entry_path);
        return -EBUSY;
    }
    if (procfs_destroy_file(procfs_file)) {
        pr_err("proc_destroy_entry(%s): Failed to remove file.\n", entry_path);
        return -ENOENT;
    }
    return 0;
}

proc_dir_entry_t *proc_create_entry(const char *name, proc_dir_entry_t *parent)
{
    char entry_path[PATH_MAX];
    strcpy(entry_path, "/proc/");
    if (parent) {
        strcat(entry_path, parent->name);
        strcat(entry_path, "/");
    }
    strcat(entry_path, name);
    // Check if the entry exists.
    if (procfs_find_entry_path(entry_path) != NULL) {
        pr_err("proc_destroy_entry(%s): Proc entry already exists.\n", entry_path);
        errno = EEXIST;
        return NULL;
    }
    // Create the new procfs file.
    procfs_file_t *procfs_file = procfs_create_file(entry_path, DT_REG);
    if (!procfs_file) {
        pr_err("proc_destroy_entry(%s): Cannot create proc entry.\n", entry_path);
        errno = ENFILE;
        return NULL;
    }
    return &procfs_file->dir_entry;
}

int proc_destroy_entry(const char *name, proc_dir_entry_t *parent)
{
    char entry_path[PATH_MAX];
    strcpy(entry_path, "/proc/");
    if (parent) {
        strcat(entry_path, parent->name);
        strcat(entry_path, "/");
    }
    strcat(entry_path, name);
    // Check if the entry exists.
    procfs_file_t *procfs_file = procfs_find_entry_path(entry_path);
    if (procfs_file == NULL) {
        pr_err("proc_destroy_entry(%s): Cannot find proc entry.\n", entry_path);
        return -ENOENT;
    }
    if ((procfs_file->flags & DT_REG) == 0) {
        pr_err("proc_destroy_entry(%s): Proc entry is not a regular file.\n", entry_path);
        return -ENOENT;
    }
    // Check if the procfs file has still some file associated.
    if (!list_head_empty(&procfs_file->files)) {
        pr_err("proc_destroy_entry(%s): Proc entry is busy.\n", entry_path);
        return -EBUSY;
    }
    if (procfs_destroy_file(procfs_file)) {
        pr_err("proc_destroy_entry(%s): Failed to remove file.\n", entry_path);
        return -ENOENT;
    }
    return 0;
}
