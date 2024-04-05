/// @file mem.c
/// @brief Memory devices.
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[MEMDEV  ]"
/// Set the log level.
#define __DEBUG_LEVEL__ GLOBAL_LOGLEVEL

#include "assert.h"
#include "drivers/mem.h"
#include "io/debug.h"
#include "fs/vfs.h"
#include "string.h"
#include "sys/errno.h"
#include "system/syscall.h"

struct memdev;

struct memdev {
    vfs_file_t *file;
    struct memdev *next;
};

static struct memdev *devices;

static void add_device(struct memdev *device) {
    struct memdev *dit = devices;
    for (;dit != NULL; dit = dit->next);
    if (dit == NULL) {
        devices = device;
    } else {
        dit->next = device;
    }
}

static vfs_file_t* find_device_file(const char *path) {
    for(struct memdev *dev = devices; dev != NULL; dev = dev->next) {
        if (strcmp(dev->file->name, path) == 0) {
            return dev->file;
        }
    }
    return NULL;
}

static int mem_stat(const char* path, stat_t *stat);

static vfs_sys_operations_t mem_sys_operations = {
    .mkdir_f = NULL,
    .rmdir_f = NULL,
    .stat_f  = mem_stat,
};

static vfs_file_t *null_open(const char *path, int flags, mode_t mode);
static int null_close(vfs_file_t * file);
static ssize_t null_write(vfs_file_t * file, const void *buffer, off_t offset, size_t size);
static ssize_t null_read(vfs_file_t * file, char *buffer, off_t offset, size_t size);
static int null_fstat(vfs_file_t * file, stat_t *stat);

static vfs_file_operations_t null_fs_operations = {
    .open_f     = null_open,
    .unlink_f   = NULL,
    .close_f    = null_close,
    .read_f     = null_read,
    .write_f    = null_write,
    .lseek_f    = NULL,
    .stat_f     = null_fstat,
    .ioctl_f    = NULL,
    .getdents_f = NULL
};

static struct memdev *null_device_create(const char* name) {
    // Create the device.
    struct memdev *dev = kmalloc(sizeof(struct memdev));
    dev->next = NULL;

    // Create the file.
    vfs_file_t *file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
    if (file == NULL) {
        pr_err("Failed to create null device.\n");
        return NULL;
    }
    dev->file = file;

    // Set the device name.
    strncpy(file->name, name, NAME_MAX);
    file->count = 0;
    file->uid = 0;
    file->gid = 0;
    file->mask = 0x2000 | 0666;
    file->atime = sys_time(NULL);
    file->mtime = sys_time(NULL);
    file->ctime = sys_time(NULL);
    file->length = 0;
    // Set the operations.
    file->sys_operations = &mem_sys_operations;
    file->fs_operations  = &null_fs_operations;
    return dev;
}

static vfs_file_t* null_open(const char *path, int flags, mode_t mode) {
    vfs_file_t* file = find_device_file(path);
    if (file) {
        file->count++;
    }
    // TODO: check permissions
    return file;
}

static int null_close(vfs_file_t * file) {
    assert(file && "Received null file.");
    file->count--;
    return 0;
}

static ssize_t null_write(vfs_file_t * file, const void *buffer, off_t offset, size_t size) {
    return size;
}

static ssize_t null_read(vfs_file_t * file, char *buffer, off_t offset, size_t size) {
    return 0;
}

static int null_fstat(vfs_file_t * file, stat_t *stat)
{
    pr_debug("null_fstat(%s, %p)\n", file->name, stat);
    stat->st_dev   = 0;
    stat->st_ino   = 0;
    stat->st_mode  = file->mask;
    stat->st_uid   = file->uid;
    stat->st_gid   = file->gid;
    stat->st_atime = file->atime;
    stat->st_mtime = file->mtime;
    stat->st_ctime = file->ctime;
    stat->st_size  = file->length;
    return 0;
}

static int mem_stat(const char *path, stat_t *stat) {
    vfs_file_t* file = find_device_file(path);

    if (file) {
        return file->fs_operations->stat_f(file, stat);
    }
    return -ENOENT;
}

int mem_devs_initialize(void)
{
    struct memdev *devnull = null_device_create("/dev/null");
    if (!devnull) {
        pr_err("Failed to create devnull");
        return -ENODEV;
    }
    
    if (!vfs_mount("/dev/null", devnull->file)) {
        pr_err("Failed to mount /dev/null");
        return 1;
    }

    add_device(devnull);

    return 0;
}
