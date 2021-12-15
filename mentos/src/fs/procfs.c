///                MentOS, The Mentoring Operating system project
/// @file procfs.c
/// @brief Proc file system implementation.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

/// Change the header.
#define __DEBUG_HEADER__ "[PROCFS]"

#include "fs/procfs.h"
#include "fs/vfs.h"
#include "string.h"
#include "sys/errno.h"
#include "io/debug.h"
#include "fcntl.h"
#include "libgen.h"
#include "assert.h"
#include "time.h"

/// Maximum length of name in PROCFS.
#define PROCFS_NAME_MAX 255U
/// Maximum number of files in PROCFS.
#define PROCFS_MAX_FILES 512U
/// The magic number used to check if the procfs file is valid.
#define PROCFS_MAGIC_NUMBER 0xBF

/// @brief Information concerning a file.
typedef struct procfs_file_t {
    /// Number used as delimiter, it must be set to 0xBF.
    int magic;
    /// The file inode.
    int inode;
    /// Flags.
    unsigned flags;
    /// The name of the file.
    char name[PROCFS_NAME_MAX];
    /// Associated files.
    list_head files;
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
} procfs_file_t;

/// @brief The details regarding the filesystem.
/// @brief Contains the number of files inside the initrd filesystem.
static struct procfs_t {
    /// Number of files.
    unsigned int nfiles;
    /// List of headers.
    procfs_file_t headers[PROCFS_MAX_FILES];
} __attribute__((aligned(16))) fs_specs;

static inline procfs_file_t *procfs_create_file(const char *path, unsigned flags);

static inline int procfs_destroy_file(procfs_file_t *procfs_file);

static inline vfs_file_t *procfs_create_file_struct(procfs_file_t *procfs_file);

static inline int procfs_get_free_inode()
{
    for (int i = 0; i < PROCFS_MAX_FILES; ++i) {
        assert(fs_specs.headers[i].magic == PROCFS_MAGIC_NUMBER);
        if (fs_specs.headers[i].inode == -1) {
            return i;
        }
    }
    return -1;
}

static inline procfs_file_t *procfs_get_free_entry()
{
    int free_inode = procfs_get_free_inode();
    if (free_inode != -1) {
        procfs_file_t *free_entry = &fs_specs.headers[free_inode];
        free_entry->inode         = free_inode;
        return free_entry;
    } else {
        pr_err("There are no more free inodes (%d/%d).\n", fs_specs.nfiles, PROCFS_MAX_FILES);
    }
    return NULL;
}

static inline procfs_file_t *procfs_find_entry_path(const char *path)
{
    for (int i = 0; i < PROCFS_MAX_FILES; ++i) {
        if (strcmp(fs_specs.headers[i].name, path) == 0) {
            return &fs_specs.headers[i];
        }
    }
    return NULL;
}

static inline procfs_file_t *procfs_find_entry_inode(int inode)
{
    for (int i = 0; i < PROCFS_MAX_FILES; ++i) {
        if (fs_specs.headers[i].inode == inode) {
            return &fs_specs.headers[i];
        }
    }
    return NULL;
}

static inline int procfs_find_inode(const char *path)
{
    procfs_file_t *file = procfs_find_entry_path(path);
    if (file)
        return file->inode;
    return -1;
}

static inline int procfs_check_if_empty(const char *path)
{
    for (int i = 0; i < PROCFS_MAX_FILES; ++i) {
        procfs_file_t *entry = &fs_specs.headers[i];
        // There is nothing here.
        if (entry->inode == -1) {
            continue;
        }
        // It's the directory itself.
        if (strcmp(path, entry->name) == 0) {
            continue;
        }
        // Get the directory of the file.
        char *filedir = dirname(entry->name);
        // Check if directory path and file directory are the same.
        if (strcmp(path, filedir) == 0) {
            return 1;
        }
    }
    return 0;
}

static void dump_procfs()
{
    for (int i = 0; i < PROCFS_MAX_FILES; ++i) {
        procfs_file_t *file = &fs_specs.headers[i];
        pr_debug("[%3d]ino:%3d, name:`%s`\n", i, file->inode, file->name);
    }
}

static void procfs_init()
{
    // Initialize the procfs.
    memset(&fs_specs, 0, sizeof(struct procfs_t));
    for (int i = 0; i < PROCFS_MAX_FILES; ++i) {
        fs_specs.headers[i].magic = PROCFS_MAGIC_NUMBER;
        fs_specs.headers[i].inode = -1;
    }
}

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

static vfs_file_t *procfs_open(const char *path, int flags, mode_t mode)
{
    procfs_file_t *procfs_file = procfs_find_entry_path(path);
    if (procfs_file != NULL) {
        // Check if it is a directory.
        if (flags == (O_RDONLY | O_DIRECTORY)) {
            if ((procfs_file->flags & DT_DIR) == 0) {
                pr_err("Is not a directory `%s`...\n", path);
                errno = ENOTDIR;
                return NULL;
            }
            // Create the associated file.
            vfs_file_t *vfs_file = procfs_create_file_struct(procfs_file);
            if (!vfs_file) {
                pr_err("Cannot create vfs file for opening directory `%s`...\n", path);
                errno = ENFILE;
                return NULL;
            }
            // Update file access.
            procfs_file->atime = sys_time(NULL);
            // Add the vfs_file to the list of associated files.
            list_head_add_tail(&vfs_file->siblings, &procfs_file->files);
            return vfs_file;
        } else if ((procfs_file->flags & DT_DIR) != 0) {
            pr_err("Is a directory `%s`...\n", path);
            errno = EISDIR;
            return NULL;
        }
        // Check if the open has to create.
        if (flags & O_CREAT) {
            pr_err("Cannot create, it exists `%s`...\n", path);
            errno = EEXIST;
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
        list_head_add_tail(&vfs_file->siblings, &procfs_file->files);
        return vfs_file;
    }
    if (flags & O_CREAT) {
        // Check if the directories before it exist.
        procfs_file_t *parent_file = NULL;
        char *parent_path          = dirname(path);
        if ((strcmp(parent_path, ".") != 0) && (strcmp(parent_path, "/") != 0)) {
            parent_file = procfs_find_entry_path(parent_path);
            if (parent_file == NULL) {
                pr_err("Cannot find parent `%s`...\n", parent_path);
                errno = ENOENT;
                return NULL;
            }
            if ((parent_file->flags & DT_DIR) == 0) {
                pr_err("Parent `%s` the parent is not a directory...\n", parent_path);
                errno = ENOTDIR;
                return NULL;
            }
        }
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
        list_head_add_tail(&vfs_file->siblings, &procfs_file->files);
        return vfs_file;
    }
    errno = ENOENT;
    return NULL;
}

static int procfs_close(vfs_file_t *file)
{
    assert(file && "Received null file.");
    //pr_debug("procfs_close(%p): VFS file : %p\n", file, file);
    // Remove the file from the list of `files` inside its corresponding `procfs_file_t`.
    list_head_del(&file->siblings);
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
    for (off_t it = 0; (it < PROCFS_MAX_FILES) && (written < count); ++it) {
        procfs_file_t *entry = &fs_specs.headers[it];
        if (entry->inode == -1) {
            continue;
        }
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
        dirp->d_ino  = it;
        dirp->d_type = entry->flags;
        strcpy(dirp->d_name, entry->name + len);
        dirp->d_off    = sizeof(dirent_t);
        dirp->d_reclen = sizeof(dirent_t);
        // Increment the written counter.
        written += sizeof(dirent_t);
        // Move to next writing position.
        dirp += 1;
    }
    return written;
}

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
    .lseek_f    = NULL,
    .stat_f     = procfs_fstat,
    .ioctl_f    = procfs_ioctl,
    .getdents_f = procfs_getdents
};

static inline procfs_file_t *procfs_create_file(const char *path, unsigned flags)
{
    procfs_file_t *file = procfs_get_free_entry();
    if (!file) {
        pr_err("Failed to get free entry (%p).\n", file);
        return NULL;
    }
    // Number used as delimiter, it must be set to 0xBF.
    assert(file->magic == PROCFS_MAGIC_NUMBER);
    // The file inode.
    assert(file->inode != -1);
    // Flags.
    file->flags = flags;
    // The name of the file.
    strcpy(file->name, path);
    // Associated files.
    list_head_init(&file->files);
    // Time of last access.
    file->atime = sys_time(NULL);
    // Time of last data modification.
    file->mtime = file->atime;
    // Time of last status change.
    file->ctime = file->atime;
    // Initialize the dir_entry.
    file->dir_entry.name           = basename(file->name);
    file->dir_entry.data           = NULL;
    file->dir_entry.sys_operations = NULL;
    file->dir_entry.fs_operations  = NULL;
    // Increase the number of files.
    ++fs_specs.nfiles;
    return file;
}

static inline int procfs_destroy_file(procfs_file_t *procfs_file)
{
    if (!procfs_file) {
        pr_err("Received a null entry (%p).\n", procfs_file);
        return 1;
    }
    // Check the number used as delimiter, it must be set to 0xBF.
    assert(procfs_file->magic == PROCFS_MAGIC_NUMBER);

    // Reset the inode.
    procfs_file->inode = -1;
    // Reset the flags.
    procfs_file->flags = 0;
    // Reset the name of the file.
    memset(procfs_file->name, 0, PROCFS_NAME_MAX);
    // Reset the list of associated files.
    list_head_init(&procfs_file->files);
    // Reset the time of last access.
    procfs_file->atime = sys_time(NULL);
    // Reset the time of last data modification.
    procfs_file->mtime = procfs_file->atime;
    // Reset the time of last status change.
    procfs_file->ctime = procfs_file->atime;

    // Decrease the number of files.
    --fs_specs.nfiles;
    return 0;
}

static inline vfs_file_t *procfs_create_file_struct(procfs_file_t *procfs_file)
{
    if (!procfs_file) {
        pr_err("procfs_create_file_struct(%p): Procfs file not valid!\n", procfs_file);
        return NULL;
    }
    vfs_file_t *file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
    if (!file) {
        pr_err("procfs_create_file_struct(%p): Failed to allocate memory for VFS file!\n", procfs_file);
        return NULL;
    }
    memset(file, 0, sizeof(vfs_file_t));

    strcpy(file->name, procfs_file->name);
    file->device         = &procfs_file->dir_entry;
    file->ino            = procfs_file->inode;
    file->uid            = 0;
    file->gid            = 0;
    file->mask           = S_IRUSR | S_IRGRP | S_IROTH;
    file->length         = 0;
    file->flags          = procfs_file->flags;
    file->sys_operations = &procfs_sys_operations;
    file->fs_operations  = &procfs_fs_operations;
    list_head_init(&file->siblings);
    //pr_debug("procfs_create_file_struct(%p): VFS file : %p\n", procfs_file, file);
    return file;
}

static vfs_file_t *procfs_mount_callback(const char *path, const char *device)
{
    // Create the new procfs file.
    procfs_file_t *procfs_file = procfs_create_file(path, DT_DIR);
    assert(procfs_file && "Failed to create procfs_file.");
    // Create the associated file.
    vfs_file_t *vfs_file = procfs_create_file_struct(procfs_file);
    assert(vfs_file && "Failed to create vfs_file.");
    // Add the vfs_file to the list of associated files.
    list_head_add_tail(&vfs_file->siblings, &procfs_file->files);
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
    procfs_init();
    // Register the filesystem.
    vfs_register_filesystem(&procfs_file_system_type);
    return 0;
}

int procfs_cleanup_module()
{
    // Unregister the filesystem.
    vfs_register_filesystem(&procfs_file_system_type);
    return 0;
}

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