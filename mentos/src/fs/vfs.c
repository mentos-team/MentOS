/// @file vfs.c
/// @brief Headers for Virtual File System (VFS).
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[VFS   ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "assert.h"
#include "fs/procfs.h"
#include "fs/vfs.h"
#include "klib/hashmap.h"
#include "klib/spinlock.h"
#include "libgen.h"
#include "process/scheduler.h"
#include "stdio.h"
#include "strerror.h"
#include "string.h"
#include "system/panic.h"
#include "system/syscall.h"

/// The hashmap that associates a type of Filesystem `name` to its `mount` function;
static hashmap_t *vfs_filesystems;
/// The list of superblocks.
static list_head vfs_super_blocks;
/// The maximum number of filesystem types.
static const unsigned vfs_filesystems_max = 10;
/// Lock for refcount field.
static spinlock_t vfs_spinlock_refcount;
/// Spinlock for the entire virtual filesystem.
static spinlock_t vfs_spinlock;
/// VFS memory cache for superblocks.
static kmem_cache_t *vfs_superblock_cache;

/// VFS memory cache for files.
kmem_cache_t *vfs_file_cache;

void vfs_init(void)
{
    // Initialize the list of superblocks.
    list_head_init(&vfs_super_blocks);
    // Initialize the caches for superblocks and files.
    vfs_superblock_cache = KMEM_CREATE(super_block_t);
    vfs_file_cache       = KMEM_CREATE(vfs_file_t);
    // Allocate the hashmap for the different filesystems.
    vfs_filesystems = hashmap_create(
        vfs_filesystems_max,
        hashmap_str_hash,
        hashmap_str_comp,
        hashmap_do_not_duplicate,
        hashmap_do_not_free);
    // Initialize the spinlock.
    spinlock_init(&vfs_spinlock);
    spinlock_init(&vfs_spinlock_refcount);
}

int vfs_register_filesystem(file_system_type *fs)
{
    if (hashmap_set(vfs_filesystems, (void *)fs->name, (void *)fs) != NULL) {
        pr_err("Filesystem already registered.\n");
        return 0;
    }
    pr_debug("vfs_register_filesystem(`%s`) : %p\n", fs->name, fs);
    return 1;
}

int vfs_unregister_filesystem(file_system_type *fs)
{
    if (hashmap_remove(vfs_filesystems, (void *)fs->name) != NULL) {
        pr_err("Filesystem not present to unregister.\n");
        return 0;
    }
    return 1;
}

super_block_t *vfs_get_superblock(const char *absolute_path)
{
    size_t last_sb_len     = 0;
    super_block_t *last_sb = NULL, *superblock = NULL;
    list_head *it;
    list_for_each (it, &vfs_super_blocks) {
        superblock = list_entry(it, super_block_t, mounts);
#if 0
        int len    = strlen(superblock->name);
        pr_debug("`%s` vs `%s`\n", absolute_path, superblock->name);
        if (!strncmp(absolute_path, superblock->name, len)) {
            size_t sbl = strlen(superblock->name);
            if (sbl > last_sb_len) {
                last_sb_len = sbl;
                last_sb     = superblock;
            }
        }
#else
        size_t len = strlen(superblock->path);
        if (!strncmp(absolute_path, superblock->path, len)) {
            if (len > last_sb_len) {
                last_sb_len = len;
                last_sb     = superblock;
            }
        }
#endif
    }
    return last_sb;
}

vfs_file_t *vfs_open(const char *path, int flags, mode_t mode)
{
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    if (!realpath(path, absolute_path, sizeof(absolute_path))) {
        pr_err("vfs_open(%s): Cannot get the absolute path!\n", path);
        errno = ENODEV;
        return NULL;
    }
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("vfs_open(%s): Cannot find the superblock!\n", path);
        errno = ENOENT;
        return NULL;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("vfs_open(%s): Cannot find the superblock root!\n", path);
        errno = ENOENT;
        return NULL;
    }
    // Rebase the absolute path.
    //size_t name_offset = (strcmp(mp->name, "/") == 0) ? 0 : strlen(mp->name);
    // Check if the function is implemented.
    if (sb_root->fs_operations->open_f == NULL) {
        pr_err("vfs_open(%s): Function not supported in current filesystem.", path);
        errno = ENOSYS;
        return NULL;
    }
    // Retrieve the file.
    vfs_file_t *file = sb_root->fs_operations->open_f(absolute_path, flags, mode);
    if (file == NULL) {
        pr_debug("vfs_open(%s): Filesystem open returned NULL file (errno: %d, %s)!\n", path, errno, strerror(errno));
        return NULL;
    }
    // Increment file reference counter.
    file->count += 1;
    // Return the file.
    return file;
}

int vfs_close(vfs_file_t *file)
{
    pr_debug("vfs_close(ino: %d, file: \"%s\", count: %d)\n", file->ino, file->name, file->count - 1);
    assert(file->count > 0);
    // Close file if it's the last reference.
    if (--file->count == 0) {
        // Check if the filesystem has the close function.
        if (file->fs_operations->close_f == NULL) {
            return -ENOSYS;
        }
        file->fs_operations->close_f(file);
    }
    return 0;
}

ssize_t vfs_read(vfs_file_t *file, void *buf, size_t offset, size_t nbytes)
{
    if (file->fs_operations->read_f == NULL) {
        pr_err("No READ function found for the current filesystem.\n");
        return -ENOSYS;
    }
    return file->fs_operations->read_f(file, buf, offset, nbytes);
}

ssize_t vfs_write(vfs_file_t *file, const void *buf, size_t offset, size_t nbytes)
{
    if (file->fs_operations->write_f == NULL) {
        pr_err("No WRITE function found for the current filesystem.\n");
        return -ENOSYS;
    }
    return file->fs_operations->write_f(file, buf, offset, nbytes);
}

off_t vfs_lseek(vfs_file_t *file, off_t offset, int whence)
{
    if (file->fs_operations->lseek_f == NULL) {
        pr_err("No WRITE function found for the current filesystem.\n");
        return -ENOSYS;
    }
    return file->fs_operations->lseek_f(file, offset, whence);
}

ssize_t vfs_getdents(vfs_file_t *file, dirent_t *dirp, off_t off, size_t count)
{
    if (file->fs_operations->getdents_f == NULL) {
        pr_err("No GETDENTS function found for the current filesystem.\n");
        return -ENOSYS;
    }
    return file->fs_operations->getdents_f(file, dirp, off, count);
}

int vfs_ioctl(vfs_file_t *file, int request, void *data)
{
    if (file->fs_operations->ioctl_f == NULL) {
        pr_err("No IOCTL function found for the current filesystem.\n");
        return -ENOSYS;
    }
    return file->fs_operations->ioctl_f(file, request, data);
}

int vfs_unlink(const char *path)
{
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    if (!realpath(path, absolute_path, sizeof(absolute_path))) {
        pr_err("vfs_unlink(%s): Cannot get the absolute path.", path);
        return -ENODEV;
    }
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("vfs_unlink(%s): Cannot find the superblock!\n", path);
        return -ENODEV;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("vfs_unlink(%s): Cannot find the superblock root.", path);
        return -ENOENT;
    }
    // Check if the function is implemented.
    if (sb_root->fs_operations->unlink_f == NULL) {
        pr_err("vfs_unlink(%s): Function not supported in current filesystem.", path);
        return -ENOSYS;
    }
    return sb_root->fs_operations->unlink_f(absolute_path);
}

int vfs_mkdir(const char *path, mode_t mode)
{
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    if (!realpath(path, absolute_path, sizeof(absolute_path))) {
        pr_err("vfs_mkdir(%s): Cannot get the absolute path.", path);
        return -ENODEV;
    }
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("vfs_mkdir(%s): Cannot find the superblock!\n");
        return -ENODEV;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("vfs_mkdir(%s): Cannot find the superblock root.", path);
        return -ENOENT;
    }
    // Check if the function is implemented.
    if (sb_root->sys_operations->mkdir_f == NULL) {
        pr_err("vfs_mkdir(%s): Function not supported in current filesystem.", path);
        return -ENOSYS;
    }
    return sb_root->sys_operations->mkdir_f(absolute_path, mode);
}

int vfs_rmdir(const char *path)
{
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    if (!realpath(path, absolute_path, sizeof(absolute_path))) {
        pr_err("vfs_rmdir(%s): Cannot get the absolute path.", path);
        return -ENODEV;
    }
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("vfs_rmdir(%s): Cannot find the superblock!\n");
        return -ENODEV;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("vfs_rmdir(%s): Cannot find the superblock root.", path);
        return -ENOENT;
    }
    // Check if the function is implemented.
    if (sb_root->sys_operations->rmdir_f == NULL) {
        pr_err("vfs_rmdir(%s): Function not supported in current filesystem.", path);
        return -ENOSYS;
    }
    // Remove the file.
    return sb_root->sys_operations->rmdir_f(absolute_path);
}

vfs_file_t *vfs_creat(const char *path, mode_t mode)
{
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    if (!realpath(path, absolute_path, sizeof(absolute_path))) {
        pr_err("vfs_creat(%s): Cannot get the absolute path.", path);
        errno = ENODEV;
        return NULL;
    }
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("vfs_creat(%s): Cannot find the superblock!\n");
        errno = ENODEV;
        return NULL;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("vfs_creat(%s): Cannot find the superblock root.", path);
        errno = ENOENT;
        return NULL;
    }
    // Check if the function is implemented.
    if (sb_root->sys_operations->creat_f == NULL) {
        pr_err("vfs_creat(%s): Function not supported in current filesystem.", path);
        errno = ENOSYS;
        return NULL;
    }
    // Retrieve the file.
    vfs_file_t *file = sb_root->sys_operations->creat_f(absolute_path, mode);
    if (file == NULL) {
        pr_err("vfs_creat(%s): Cannot find the given file (%s)!\n", path, strerror(errno));
        errno = ENOENT;
        return NULL;
    }
    // Increment file reference counter.
    file->count += 1;
    // Return the file.
    return file;
}

ssize_t vfs_readlink(vfs_file_t *file, char *buffer, size_t bufsize)
{
    if (file == NULL) {
        pr_err("vfs_readlink: received a null pointer for file.\n");
        return -ENOENT;
    }
    if (file->fs_operations->readlink_f == NULL) {
        pr_err("vfs_readlink(%s): Function not supported in current filesystem.", file->name);
        return -ENOSYS;
    }
    // Perform the read.
    return file->fs_operations->readlink_f(file, buffer, bufsize);
}

int vfs_symlink(const char *linkname, const char *path)
{
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    if (!realpath(linkname, absolute_path, sizeof(absolute_path))) {
        pr_err("vfs_symlink(%s, %s): Cannot get the absolute path.", linkname, path);
        return -ENODEV;
    }
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("vfs_symlink(%s, %s): Cannot find the superblock!\n");
        return -ENODEV;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("vfs_symlink(%s, %s): Cannot find the superblock root.", linkname, path);
        return -ENOENT;
    }
    // Check if the function is implemented.
    if (sb_root->sys_operations->symlink_f == NULL) {
        pr_err("vfs_symlink(%s, %s): Function not supported in current filesystem.", linkname, path);
        return -ENOSYS;
    }
    pr_alert("vfs_symlink(%s, %s)", linkname, path);
    return 0;
}

int vfs_stat(const char *path, stat_t *buf)
{
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    if (!realpath(path, absolute_path, sizeof(absolute_path))) {
        pr_err("vfs_stat(%s): Cannot get the absolute path.", path);
        return -ENODEV;
    }
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("vfs_stat(%s): Cannot find the superblock!\n");
        return -ENODEV;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("vfs_stat(%s): Cannot find the superblock root.", path);
        return -ENOENT;
    }
    // Check if the function is implemented.
    if (sb_root->sys_operations->stat_f == NULL) {
        pr_err("vfs_stat(%s): Function not supported in current filesystem.", path);
        return -ENOSYS;
    }
    // Reset the structure.
    buf->st_dev   = 0;
    buf->st_ino   = 0;
    buf->st_mode  = 0;
    buf->st_uid   = 0;
    buf->st_gid   = 0;
    buf->st_size  = 0;
    buf->st_atime = 0;
    buf->st_mtime = 0;
    buf->st_ctime = 0;
    // Retrieve the file.
    return sb_root->sys_operations->stat_f(absolute_path, buf);
}

int vfs_fstat(vfs_file_t *file, stat_t *buf)
{
    if (file->fs_operations->stat_f == NULL) {
        pr_err("No FSTAT function found for the current filesystem.\n");
        return -ENOSYS;
    }
    // Reset the structure.
    buf->st_dev   = 0;
    buf->st_ino   = 0;
    buf->st_mode  = 0;
    buf->st_uid   = 0;
    buf->st_gid   = 0;
    buf->st_size  = 0;
    buf->st_atime = 0;
    buf->st_mtime = 0;
    buf->st_ctime = 0;
    return file->fs_operations->stat_f(file, buf);
}

int vfs_mount(const char *path, vfs_file_t *new_fs_root)
{
    if (!path || path[0] != '/') {
        pr_err("vfs_mount(%s): Path must be absolute for superblock.\n", path);
        return 0;
    }
    if (new_fs_root == NULL) {
        pr_err("vfs_mount(%s): You must provide a valid file!\n", path);
        return 0;
    }
    // Lock the vfs spinlock.
    spinlock_lock(&vfs_spinlock);
    pr_debug("Mounting file with path `%s` as root '%s'...\n", new_fs_root->name, path);
    // Create the superblock.
    super_block_t *sb = kmem_cache_alloc(vfs_superblock_cache, GFP_KERNEL);
    if (!sb) {
        pr_debug("Cannot allocate memory for the superblock.\n");
    } else {
        // Copy the name.
        strcpy(sb->name, new_fs_root->name);
        // Copy the path.
        strcpy(sb->path, path);
        // Set the pointer.
        sb->root = new_fs_root;
        // Add to the list.
        list_head_insert_after(&sb->mounts, &vfs_super_blocks);
    }
    spinlock_unlock(&vfs_spinlock);
    pr_debug("Correctly mounted '%s' on '%s'...\n", new_fs_root->name, path);
    return 1;
}

int do_mount(const char *type, const char *path, const char *args)
{
    file_system_type *fst = (file_system_type *)hashmap_get(vfs_filesystems, type);
    if (fst == NULL) {
        pr_err("Unknown filesystem type: %s\n", type);
        return -ENODEV;
    }
    if (fst->mount == NULL) {
        pr_err("No mount callback set: %s\n", type);
        return -ENODEV;
    }
    vfs_file_t *file = fst->mount(path, args);
    if (file == NULL) {
        pr_err("Mount callback return a null pointer: %s\n", type);
        return -ENODEV;
    }
    if (!vfs_mount(path, file)) {
        pr_err("do_mount(`%s`, `%s`, `%s`) : failed to mount.\n", type, path, args);
        return -ENODEV;
    }
    super_block_t *sb = vfs_get_superblock(path);
    if (sb == NULL) {
        pr_err("do_mount(`%s`, `%s`, `%s`) : Cannot find the superblock.\n", type, path, args);
        return -ENODEV;
    }
    // Set the filesystem type.
    sb->type = fst;
    pr_debug("Mounted %s[%s] to `%s`: file = %p\n", type, args, path, file);
    return 0;
}

void vfs_lock(vfs_file_t *file)
{
    spinlock_lock(&vfs_spinlock_refcount);
    file->refcount = -1;
    spinlock_unlock(&vfs_spinlock_refcount);
}

int vfs_extend_task_fd_list(struct task_struct *task)
{
    if (!task) {
        pr_err("Null process.\n");
        errno = ESRCH;
        return 0;
    }
    // Set the max number of file descriptors.
    int new_max_fd = (task->fd_list) ? task->max_fd * 2 + 1 : MAX_OPEN_FD;
    // Allocate the memory for the list.
    void *new_fd_list = kmalloc(new_max_fd * sizeof(vfs_file_descriptor_t));
    // Check the new list.
    if (!new_fd_list) {
        pr_err("Failed to allocate memory for `fd_list`.\n");
        errno = EMFILE;
        return 0;
    }
    // Clear the memory of the new list.
    memset(new_fd_list, 0, task->max_fd * sizeof(vfs_file_descriptor_t));
    // Deal with pre-existing list.
    if (task->fd_list) {
        // Copy the old entries.
        memcpy(new_fd_list, task->fd_list, task->max_fd * sizeof(vfs_file_descriptor_t));
        // Free the memory of the old list.
        kfree(task->fd_list);
    }
    // Set the new maximum number of file descriptors.
    task->max_fd = new_max_fd;
    // Set the new list.
    task->fd_list = new_fd_list;
    return 1;
}

int vfs_init_task(task_struct *task)
{
    if (!task) {
        pr_err("Null process.\n");
        errno = ESRCH;
        return 0;
    }
    // Initialize the file descriptor list.
    if (!vfs_extend_task_fd_list(task)) {
        pr_err("Error while trying to initialize the `fd_list` for process `%d`: %s\n", task->pid, strerror(errno));
        return 0;
    }
    // Create the proc entry.
    if (procr_create_entry_pid(task)) {
        pr_err("Error while trying to create proc entry for process `%d`: %s\n", task->pid, strerror(errno));
        return 0;
    }
    return 1;
}

int vfs_dup_task(task_struct *task, task_struct *old_task)
{
    // Copy the maximum number of file descriptors.
    task->max_fd = old_task->max_fd;
    // Allocate the memory for the new list.
    task->fd_list = kmalloc(task->max_fd * sizeof(vfs_file_descriptor_t));
    // Copy the old list.
    memcpy(task->fd_list, old_task->fd_list, task->max_fd * sizeof(vfs_file_descriptor_t));
    // Increase the counters to the open files.
    for (int fd = 0; fd < task->max_fd; fd++) {
        // Check if the file descriptor is associated with a file.
        if (task->fd_list[fd].file_struct) {
            // Increase the counter.
            ++task->fd_list[fd].file_struct->count;
        }
    }
    // Create the proc entry.
    if (procr_create_entry_pid(task)) {
        pr_err("Error while trying to create proc entry for '%d': %s\n", task->pid, strerror(errno));
        return 0;
    }
    return 1;
}

int vfs_destroy_task(task_struct *task)
{
    // Decrease the counters to the open files.
    for (int fd = 0; fd < task->max_fd; fd++) {
        // Check if the file descriptor is associated with a file.
        if (task->fd_list[fd].file_struct) {
            // Decrease the counter.
            --task->fd_list[fd].file_struct->count;
            // If counter is zero, close the file.
            if (task->fd_list[fd].file_struct->count == 0) {
                task->fd_list[fd].file_struct->fs_operations->close_f(task->fd_list[fd].file_struct);
            }
            // Clear the pointer to the file structure.
            task->fd_list[fd].file_struct = NULL;
        }
    }
    // Set the maximum file descriptors to 0.
    task->max_fd = 0;
    // Free the memory of the list.
    kfree(task->fd_list);
    // Remove the proc entry.
    if (procr_destroy_entry_pid(task)) {
        pr_err("Error while trying to remove proc entry for '%d': %s\n", task->pid, strerror(errno));
        return 0;
    }
    return 1;
}

int get_unused_fd(void)
{
    // Get the current task.
    task_struct *task = scheduler_get_current_process();

    // Search for an unused fd.
    int fd;
    for (fd = 0; fd < task->max_fd; ++fd) {
        if (!task->fd_list[fd].file_struct) {
            break;
        }
    }

    // Check if there is not fd available.
    if (fd >= MAX_OPEN_FD) {
        return -EMFILE;
    }

    // If fd limit is reached, try to allocate more
    if (fd == task->max_fd) {
        if (!vfs_extend_task_fd_list(task)) {
            pr_err("Failed to extend the file descriptor list.\n");
            return -EMFILE;
        }
    }

    return fd;
}

int sys_dup(int fd)
{
    // Get the current task.
    task_struct *task = scheduler_get_current_process();

    // Check the current FD.
    if (fd < 0 || fd >= task->max_fd) {
        return -EMFILE;
    }

    // Get the file descriptor.
    vfs_file_descriptor_t *vfd = &task->fd_list[fd];
    vfs_file_t *file = vfd->file_struct;

    // Check the file.
    if (file == NULL) {
        return -ENOSYS;
    }

    fd = get_unused_fd();
    if (fd < 0)
        return fd;

    // Increment file reference counter.
    file->count += 1;

    // Install the new fd
    task->fd_list[fd].file_struct = file;
    task->fd_list[fd].flags_mask = vfd->flags_mask;

    return fd;
}
