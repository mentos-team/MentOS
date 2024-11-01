/// @file vfs.c
/// @brief Headers for Virtual File System (VFS).
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[VFS   ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "fcntl.h"
#include "sys/stat.h"
#include "assert.h"
#include "fs/procfs.h"
#include "fs/namei.h"
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
#include "fs/pipe.h"

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

int vfs_register_filesystem(file_system_type_t *fs)
{
    if (hashmap_set(vfs_filesystems, fs->name, fs) != NULL) {
        pr_err("Filesystem already registered.\n");
        return 0;
    }
    pr_debug("vfs_register_filesystem(name: %s)\n", fs->name);
    return 1;
}

int vfs_unregister_filesystem(file_system_type_t *fs)
{
    if (hashmap_remove(vfs_filesystems, (void *)fs->name) != NULL) {
        pr_err("Filesystem not present to unregister.\n");
        return 0;
    }
    pr_debug("vfs_unregister_filesystem(name: %s)\n", fs->name);
    return 1;
}

int vfs_register_superblock(const char *name, const char *path, file_system_type_t *type, vfs_file_t *root)
{
    pr_debug("vfs_register_superblock(name: %s, path: %s, type: %s)\n", name, path, type->name);
    // Lock the vfs spinlock.
    spinlock_lock(&vfs_spinlock);
    // Create the superblock.
    super_block_t *sb = kmem_cache_alloc(vfs_superblock_cache, GFP_KERNEL);
    // Check if the superblock was correctly allocated.
    assert(sb && "Cannot allocate memory for the superblock.\n");
    // Copy the name.
    strcpy(sb->name, name);
    // Copy the path.
    strcpy(sb->path, path);
    // Set the pointer.
    sb->root = root;
    // Set the type.
    sb->type = type;
    // Add the superblock to the list.
    list_head_insert_after(&sb->mounts, &vfs_super_blocks);

    pr_debug("Superblocks:\n");
    list_for_each_decl(it, &vfs_super_blocks)
    {
        super_block_t *_sb = list_entry(it, super_block_t, mounts);
        pr_debug("    Name: %-12s, Path: %-12s, Type: %-12s\n", _sb->name, _sb->path, _sb->type->name);
    }
    pr_debug("\n");

    // Unlock the vfs spinlock.
    spinlock_unlock(&vfs_spinlock);
    return 1;
}

int vfs_unregister_superblock(super_block_t *sb)
{
    pr_debug("vfs_unregister_superblock(name: %s, path: %s, type: %s)\n", sb->name, sb->path, sb->type->name);
    list_head_remove(&sb->mounts);
    kmem_cache_free(sb);
    return 1;
}

super_block_t *vfs_get_superblock(const char *path)
{
    pr_debug("vfs_get_superblock(path: %s)\n", path);
    size_t last_sb_len     = 0, len;
    super_block_t *last_sb = NULL, *sb = NULL;
    list_head *it;
    list_for_each (it, &vfs_super_blocks) {
        sb  = list_entry(it, super_block_t, mounts);
        len = strlen(sb->path);
        if (!strncmp(path, sb->path, len)) {
            if (len > last_sb_len) {
                last_sb_len = len;
                last_sb     = sb;
            }
        }
    }
    return last_sb;
}

vfs_file_t *vfs_open_abspath(const char *absolute_path, int flags, mode_t mode)
{
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("vfs_open_abspath(%s): Cannot find the superblock!\n", absolute_path);
        errno = ENOENT;
        return NULL;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("vfs_open_abspath(%s): Cannot find the superblock root!\n", absolute_path);
        errno = ENOENT;
        return NULL;
    }
    // Rebase the absolute path.
    //size_t name_offset = (strcmp(mp->name, "/") == 0) ? 0 : strlen(mp->name);
    // Check if the function is implemented.
    if (sb_root->fs_operations->open_f == NULL) {
        pr_err("vfs_open_abspath(%s): Function not supported in current filesystem.\n", absolute_path);
        errno = ENOSYS;
        return NULL;
    }
    // Retrieve the file.
    vfs_file_t *file = sb_root->fs_operations->open_f(absolute_path, flags, mode);
    if (file == NULL) {
        pr_debug("vfs_open_abspath(%s): Filesystem open returned NULL file (errno: %d, %s)!\n",
                 absolute_path, errno, strerror(errno));
        return NULL;
    }
    // Increment file reference counter.
    file->count += 1;
    // Return the file.
    return file;
}

vfs_file_t *vfs_open(const char *path, int flags, mode_t mode)
{
    pr_debug("vfs_open(path: %s, flags: %d, mode: %d)\n", path, flags, mode);
    assert(path && "Provided null path.");
    // Resolve all symbolic links in the path before opening the file.
    int resolve_flags = FOLLOW_LINKS | REMOVE_TRAILING_SLASH;
    // Allow the last component to be non existing when attempting to create it.
    if (bitmask_check(flags, O_CREAT)) {
        resolve_flags |= CREAT_LAST_COMPONENT;
    }
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    int ret = resolve_path(path, absolute_path, PATH_MAX, resolve_flags);
    if (ret < 0) {
        pr_err("vfs_open(%s): Cannot resolve path!\n", path);
        errno = -ret;
        return NULL;
    }
    pr_debug("vfs_open(path: %s, flags: %d, mode: %d) -> %s\n", path, flags, mode, absolute_path);
    return vfs_open_abspath(absolute_path, flags, mode);
}

int vfs_close(vfs_file_t *file)
{
    // Check for null file pointer.
    if (file == NULL) {
        pr_err("vfs_close: Invalid file pointer (NULL).\n");
        return -EINVAL;
    }

    // Check for valid fs_operations pointer.
    if (file->fs_operations == NULL) {
        pr_err("vfs_close: No fs_operations provided for file \"%s\" (ino: %d).\n", file->name, file->ino);
        return -EFAULT;
    }

    pr_debug("vfs_close(ino: %d, file: \"%s\", count: %d)\n", file->ino, file->name, file->count - 1);

    // Ensure reference count is greater than zero.
    if (file->count <= 0) {
        pr_crit("vfs_close: Invalid reference count (%d) for file \"%s\" (ino: %d).\n", file->count, file->name, file->ino);
        return -EINVAL;
    }

    // Check if the filesystem has a close function.
    if (file->fs_operations->close_f == NULL) {
        pr_warning("vfs_close: Filesystem does not support close operation for file \"%s\" (ino: %d).\n", file->name, file->ino);
        return -ENOSYS;
    }

    int ret = file->fs_operations->close_f(file);
    if (ret < 0) {
        pr_err("vfs_close: Filesystem close function failed for file \"%s\" (ino: %d) with error %d.\n", file->name, file->ino, ret);
        return ret; // Return the specific error from close_f
    }

    pr_debug("vfs_close: Successfully closed file \"%s\" (ino: %d).\n", file->name, file->ino);

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
    int resolve_flags = REMOVE_TRAILING_SLASH | FOLLOW_LINKS;
    int ret           = resolve_path(path, absolute_path, PATH_MAX, resolve_flags);
    if (ret < 0) {
        pr_err("vfs_unlink(%s): Cannot get the absolute path.\n", path);
        return ret;
    }
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("vfs_unlink(%s): Cannot find the superblock!\n", path);
        return -ENODEV;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("vfs_unlink(%s): Cannot find the superblock root.\n", path);
        return -ENOENT;
    }
    // Check if the function is implemented.
    if (sb_root->fs_operations->unlink_f == NULL) {
        pr_err("vfs_unlink(%s): Function not supported in current filesystem.\n", path);
        return -ENOSYS;
    }
    return sb_root->fs_operations->unlink_f(absolute_path);
}

int vfs_mkdir(const char *path, mode_t mode)
{
    pr_debug("vfs_mkdir(path: %s, mode: %d)\n", path, mode);
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    int resolve_flags = REMOVE_TRAILING_SLASH | FOLLOW_LINKS | CREAT_LAST_COMPONENT;
    int ret           = resolve_path(path, absolute_path, PATH_MAX, resolve_flags);
    if (ret < 0) {
        pr_err("vfs_mkdir(%s): Cannot get the absolute path.\n", path);
        return ret;
    }
    pr_debug("vfs_mkdir(path: %s, mode: %d) -> absolute_path: %s\n", path, mode, absolute_path);
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("vfs_mkdir(%s): Cannot find the superblock!\n");
        return -ENODEV;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("vfs_mkdir(%s): Cannot find the superblock root.\n", path);
        return -ENOENT;
    }
    // Check if the function is implemented.
    if (sb_root->sys_operations->mkdir_f == NULL) {
        pr_err("vfs_mkdir(%s): Function not supported in current filesystem.\n", path);
        return -ENOSYS;
    }
    return sb_root->sys_operations->mkdir_f(absolute_path, mode);
}

int vfs_rmdir(const char *path)
{
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    int resolve_flags = REMOVE_TRAILING_SLASH | FOLLOW_LINKS;
    int ret           = resolve_path(path, absolute_path, PATH_MAX, resolve_flags);
    if (ret < 0) {
        pr_err("vfs_rmdir(%s): Cannot get the absolute path.\n", path);
        return ret;
    }
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("vfs_rmdir(%s): Cannot find the superblock!\n");
        return -ENODEV;
    }
    vfs_file_t *sb_root = sb->root;
    if (sb_root == NULL) {
        pr_err("vfs_rmdir(%s): Cannot find the superblock root.\n", path);
        return -ENOENT;
    }
    // Check if the function is implemented.
    if (sb_root->sys_operations->rmdir_f == NULL) {
        pr_err("vfs_rmdir(%s): Function not supported in current filesystem.\n", path);
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
    int resolve_flags = REMOVE_TRAILING_SLASH | FOLLOW_LINKS;
    int ret           = resolve_path(path, absolute_path, PATH_MAX, resolve_flags);
    if (ret < 0) {
        pr_err("vfs_creat(%s): Cannot get the absolute path.\n", path);
        errno = ret;
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
        pr_err("vfs_creat(%s): Cannot find the superblock root.\n", path);
        errno = ENOENT;
        return NULL;
    }
    // Check if the function is implemented.
    if (sb_root->sys_operations->creat_f == NULL) {
        pr_err("vfs_creat(%s): Function not supported in current filesystem.\n", path);
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

ssize_t vfs_readlink(const char *path, char *buffer, size_t bufsize)
{
    pr_debug("vfs_readlink(%s, %s, %d)\n", path, buffer, bufsize);
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX] = { 0 };
    // If the first character is not the '/' then get the absolute path.
    int ret = resolve_path(path, absolute_path, PATH_MAX, REMOVE_TRAILING_SLASH);
    if (ret < 0) {
        pr_err("vfs_readlink(%s, %s, %d): Cannot get the absolute path.", path, buffer, bufsize);
        return ret;
    }
    super_block_t *sb = vfs_get_superblock(absolute_path);
    if (sb == NULL) {
        pr_err("vfs_readlink(%s, %s, %d): Cannot find the superblock!.\n", path, buffer, bufsize);
        return -ENOENT;
    }
    if (sb->root == NULL) {
        pr_err("vfs_readlink(%s, %s, %d): Cannot find the superblock root.\n", path, buffer, bufsize);
        return -ENOENT;
    }
    if (sb->root->fs_operations->readlink_f == NULL) {
        return -ENOENT;
    }
    // Perform the read.
    return sb->root->fs_operations->readlink_f(absolute_path, buffer, bufsize);
}

int vfs_symlink(const char *linkname, const char *path)
{
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    int resolve_flags = REMOVE_TRAILING_SLASH | FOLLOW_LINKS;
    int ret           = resolve_path(path, absolute_path, PATH_MAX, resolve_flags);
    if (ret < 0) {
        pr_err("vfs_symlink(%s, %s): Cannot get the absolute path.", linkname, path);
        return ret;
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
    pr_debug("vfs_stat(path: %s, buf: %p)\n", path, buf);
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX] = { 0 };
    // If the first character is not the '/' then get the absolute path.
    int ret = resolve_path(path, absolute_path, PATH_MAX, REMOVE_TRAILING_SLASH);
    if (ret < 0) {
        pr_err("vfs_stat(%s): Cannot get the absolute path.", path);
        return ret;
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
    memset(buf, 0, sizeof(stat_t));
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

int vfs_mount(const char *type, const char *path, const char *args)
{
    file_system_type_t *fst = (file_system_type_t *)hashmap_get(vfs_filesystems, type);
    if (fst == NULL) {
        pr_err("Unknown filesystem type: %s\n", type);
        return -ENODEV;
    }
    if (fst->mount == NULL) {
        pr_err("No mount callback set: %s\n", type);
        return -ENODEV;
    }
    // Allocate a variable for the path.
    char absolute_path[PATH_MAX];
    // If the first character is not the '/' then get the absolute path.
    int resolve_flags = 0;
    int ret           = resolve_path(args, absolute_path, PATH_MAX, resolve_flags);
    if (ret < 0) {
        pr_err("vfs_mount(type: %s, path: %s, args: %s): Cannot get the absolute path\n",
               fst->name, path, args);
        return ret;
    }
    pr_debug("vfs_mount(type: %s, path: %s, args: %s (%s))\n", fst->name, path, args, absolute_path);
    vfs_file_t *file = fst->mount(path, absolute_path);
    if (file == NULL) {
        pr_err("Mount callback return a null pointer: %s\n", type);
        return -ENODEV;
    }
    // Register the proc superblock.
    if (!vfs_register_superblock(file->name, path, fst, file)) {
        pr_alert("Failed to register %s superblock!\n", file->name);
        return -ENODEV;
    }
    pr_debug("vfs_mount(type: %s, path: %s, args: %s), file: %s\n", fst->name, path, args, file->name);
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
    if (vfs_update_pipe_counts(task, old_task)) {
        pr_err("Error while updating the pipe count for '%d': %s\n", task->pid, strerror(errno));
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
    vfs_file_t *file           = vfd->file_struct;

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
    task->fd_list[fd].flags_mask  = vfd->flags_mask;

    return fd;
}

/// @brief Checks the valid open permission.
/// @param mask the mask we need to check against.
/// @param flags the flags we want to check.
/// @param read the read permissions we want to check.
/// @param write the write permissions we want to check.
/// @return 0 on falure, success otherwise.
static inline int __valid_open_permissions(
    const mode_t mask,
    const int flags,
    const int read,
    const int write)
{
    if ((flags & O_ACCMODE) == O_RDONLY) {
        return mask & read;
    }
    if ((flags & O_ACCMODE) == O_WRONLY) {
        return mask & write;
    }
    if ((flags & O_ACCMODE) == O_RDWR) {
        return mask & (write | read);
    }
    return 0;
}

int vfs_valid_open_permissions(int flags, mode_t mask, uid_t uid, gid_t gid)
{
    // Check the permissions.
    task_struct *task = scheduler_get_current_process();
    if (task == NULL) {
        pr_warning("Failed to get the current running process, assuming we are booting.\n");
        return 1;
    }
    // Init, and all root processes have full permissions.
    if ((task->pid == 0) || (task->uid == 0)) {
        return 1;
    }
    // Check the owners permission
    if (task->uid == uid) {
        return __valid_open_permissions(mask, flags, S_IRUSR, S_IWUSR);
        // Check the groups permission
    } else if (task->gid == gid) {
        return __valid_open_permissions(mask, flags, S_IRGRP, S_IWGRP);
    }

    // Check the others permission
    return __valid_open_permissions(mask, flags, S_IROTH, S_IWOTH);
}

int vfs_valid_exec_permission(task_struct *task, vfs_file_t *file)
{
    // Init, and all root processes may execute any file with an execute bit set
    if ((task->pid == 0) || (task->uid == 0)) {
        return file->mask & (S_IXUSR | S_IXGRP | S_IXOTH);
    }

    // Check the owners permission
    if (task->uid == file->uid) {
        return file->mask & S_IXUSR;
        // Check the groups permission
    } else if (task->gid == file->gid) {
        return file->mask & S_IXGRP;
    }

    // Check the others permission
    return file->mask & S_IXOTH;
}
