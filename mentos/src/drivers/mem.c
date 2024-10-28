/// @file mem.c
/// @brief Memory devices.
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[MEMDEV]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "assert.h"
#include "drivers/mem.h"
#include "io/debug.h"
#include "fs/vfs.h"
#include "string.h"
#include "sys/errno.h"
#include "system/syscall.h"
#include "process/scheduler.h"
#include "fcntl.h"
#include "sys/stat.h"

/// @brief Structure representing a memory device.
struct memdev {
    vfs_file_t *file;    ///< Pointer to the associated file.
    struct memdev *next; ///< Pointer to the next memory device in the list.
};

/// @brief Head of the linked list of memory devices.
static struct memdev *devices;

/// @brief Adds a memory device to the linked list.
///
/// @param device Pointer to the memory device to add.
/// @return 0 on success, -EINVAL if the device is NULL.
static int add_device(struct memdev *device)
{
    // Check if the device is NULL.
    if (device == NULL) {
        pr_crit("add_device: NULL device provided\n");
        return -EINVAL; // Return error for NULL device.
    }
    struct memdev *dit = devices;
    // Traverse to the end of the list.
    for (; dit && dit->next; dit = dit->next);
    // Add the device to the list.
    if (dit == NULL) {
        devices = device;
    } else {
        dit->next = device;
    }
    return 0; // Return success.
}

/// @brief Finds a device file by its path.
///
/// @details This function traverses the list of memory devices to find a file
/// that matches the provided path.
///
/// @param path The path of the file to search for.
/// @return A pointer to the vfs_file_t if found, otherwise NULL.
static vfs_file_t *find_device_file(const char *path)
{
    // Check if the path is NULL.
    if (path == NULL) {
        pr_crit("find_device_file: NULL path provided\n");
        return NULL;
    }
    // Traverse the linked list of devices.
    for (struct memdev *dev = devices; dev != NULL; dev = dev->next) {
        // Check if the file is NULL to avoid dereferencing a NULL pointer.
        if (dev->file == NULL) {
            pr_crit("find_device_file: NULL file found in device\n");
            continue;
        }
        // Compare the file name with the provided path.
        if (strcmp(dev->file->name, path) == 0) {
            return dev->file;
        }
    }
    // Return NULL if no device file matches the given path.
    return NULL;
}

static int mem_stat(const char *path, stat_t *stat);

/// @brief System operations for the memory device.
static vfs_sys_operations_t mem_sys_operations = {
    .mkdir_f = NULL,
    .rmdir_f = NULL,
    .stat_f  = mem_stat,
};

static vfs_file_t *null_mount_callback(const char *path, const char *device);
static vfs_file_t *null_open(const char *path, int flags, mode_t mode);
static int null_close(vfs_file_t *file);
static ssize_t null_write(vfs_file_t *file, const void *buffer, off_t offset, size_t size);
static ssize_t null_read(vfs_file_t *file, char *buffer, off_t offset, size_t size);
static int null_fstat(vfs_file_t *file, stat_t *stat);

/// @brief Filesystem type structure for the null device.
static file_system_type null_file_system_type = {
    .name     = "null",
    .fs_flags = 0,
    .mount    = null_mount_callback
};

/// @brief File operations for the null device.
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

/// @brief Retrieves the status information of a memory device file.
///
/// @details This function retrieves the file status (e.g., size, permissions)
/// for a memory device file specified by the given path.
///
/// @param path The path of the memory device file.
/// @param stat A pointer to a stat_t structure where the file status will be stored.
/// @return 0 on success, -ENOENT if the file is not found.
static int mem_stat(const char *path, stat_t *stat)
{
    // Check if the path or stat pointer is NULL.
    if (path == NULL) {
        pr_crit("mem_stat: NULL path provided\n");
        return -EINVAL;
    }
    if (stat == NULL) {
        pr_crit("mem_stat: NULL stat pointer provided\n");
        return -EINVAL;
    }
    // Find the memory device file by path.
    vfs_file_t *file = find_device_file(path);
    // Check if the file was found.
    if (file != NULL) {
        // Call the stat_f operation to retrieve the file status.
        return file->fs_operations->stat_f(file, stat);
    }
    // Return -ENOENT if the file was not found.
    return -ENOENT;
}

/// @brief The mount callback, which prepares everything and calls the actual
/// NULL mount function.
///
/// @details This function serves as a callback for mounting a NULL filesystem.
/// Since the NULL device does not support mounting, this function logs an error
/// and returns NULL.
///
/// @param path The path where the filesystem should be mounted.
/// @param device The device to be mounted (unused in this case as NULL has no mount functionality).
/// @return Always returns NULL as mounting is not supported for NULL devices.
static vfs_file_t *null_mount_callback(const char *path, const char *device)
{
    // Log an error indicating that NULL devices do not support mounting.
    pr_err("null_mount_callback(%s, %s): NULL device does not support mounting!\n", path, device);
    return NULL;
}

/// @brief Creates a null memory device.
///
/// @details This function allocates memory for a new null device and its associated
/// file, initializes the file structure, and sets appropriate operations.
///
/// @param name The name of the null device to be created.
/// @return A pointer to the newly created memory device, or NULL if creation fails.
static struct memdev *null_device_create(const char *name)
{
    // Check if the name is NULL.
    if (name == NULL) {
        pr_err("null_device_create: NULL name provided\n");
        return NULL;
    }

    // Allocate memory for the new device.
    struct memdev *dev = kmalloc(sizeof(struct memdev));
    if (dev == NULL) {
        pr_err("null_device_create: Failed to allocate memory for device\n");
        return NULL;
    }
    dev->next = NULL;

    // Allocate memory for the associated file.
    vfs_file_t *file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
    if (file == NULL) {
        pr_err("null_device_create: Failed to allocate memory for file\n");
        kfree(dev); // Free the previously allocated device memory.
        return NULL;
    }
    dev->file = file;

    // Set the device name, ensuring it doesn't exceed NAME_MAX.
    strncpy(file->name, name, NAME_MAX - 1);
    file->name[NAME_MAX - 1] = '\0'; // Ensure null termination.

    // Initialize file fields.
    file->count  = 0;
    file->uid    = 0;
    file->gid    = 0;
    file->mask   = 0x2000 | 0666;  // Regular file with rw-rw-rw- permissions.
    file->atime  = sys_time(NULL); // Set access time.
    file->mtime  = sys_time(NULL); // Set modification time.
    file->ctime  = sys_time(NULL); // Set change time.
    file->length = 0;              // Initialize file length to 0.

    // Set the file system and system operations for the file.
    file->sys_operations = &mem_sys_operations;
    file->fs_operations  = &null_fs_operations;

    return dev;
}

/// @brief Opens a null device file.
///
/// @param path The path of the null device file to open.
/// @param flags Flags specifying the access mode and other options for opening the file.
/// @param mode The mode specifying permissions for the file (not currently used).
/// @return A pointer to the opened vfs_file_t structure, or NULL if the file is not found.
static vfs_file_t *null_open(const char *path, int flags, mode_t mode)
{
    // Check if the path is NULL.
    if (path == NULL) {
        pr_crit("null_open: NULL path provided\n");
        return NULL;
    }

    // Find the null device file by its path.
    vfs_file_t *file = find_device_file(path);
    if (file == NULL) {
        // Log an error if the file was not found.
        pr_crit("null_open: File not found for path %s\n", path);
        return NULL;
    }

    // Get the current task.
    task_struct *task = scheduler_get_current_process();

    // Check read or write access based on the flags.
    if (flags & O_RDONLY) {
        // Check read permission.
        if (((file->uid == task->uid) && !(file->mask & S_IRUSR)) || // Check owner read permission.
            ((file->gid == task->gid) && !(file->mask & S_IRGRP)) || // Check group read permission.
            (!(file->mask & S_IROTH))) {                             // Check others read permission.
            pr_err("null_open: Permission denied for read access to %s\n", path);
            return NULL;
        }
    }

    if (flags & O_WRONLY || flags & O_RDWR) {
        // Check write permission.
        if (((file->uid == task->uid) && !(file->mask & S_IWUSR)) || // Check owner write permission.
            ((file->gid == task->gid) && !(file->mask & S_IWGRP)) || // Check group write permission.
            (!(file->mask & S_IWOTH))) {                             // Check others write permission.
            pr_err("null_open: Permission denied for write access to %s\n", path);
            return NULL;
        }
    }

    // Increment the reference count for the file.
    file->count++;

    return file;
}

/// @brief Closes a null device file.
///
/// @param file A pointer to the vfs_file_t structure representing the file to close.
/// @return 0 on success, -EINVAL if the file is NULL.
static int null_close(vfs_file_t *file)
{
    // Check if the file is NULL.
    if (file == NULL) {
        pr_crit("null_close: Received NULL file\n");
        return -EINVAL; // Return an error if the file is NULL.
    }

    // Decrease the reference count.
    if (file->count > 0) {
        file->count--;
    } else {
        pr_warning("null_close: Attempt to close a file with zero reference count\n");
    }

    return 0;
}
/// @brief Writes data to a null device file.
///
/// @param file A pointer to the vfs_file_t structure representing the file to write to.
/// @param buffer The buffer containing the data to write (unused).
/// @param offset The offset from where the write operation should begin (unused).
/// @param size The number of bytes to write.
/// @return The size of the data that would have been written, or -EINVAL if the file is NULL.
static ssize_t null_write(vfs_file_t *file, const void *buffer, off_t offset, size_t size)
{
    // Check if the file is NULL.
    if (file == NULL) {
        pr_crit("null_write: Received NULL file\n");
        return -EINVAL; // Return an error if the file is NULL.
    }

    // Since this is a null device, the data is discarded.
    return size; // Simulate a successful write by returning the size.
}

/// @brief Reads data from a null device file.
///
/// @param file A pointer to the vfs_file_t structure representing the file to read from.
/// @param buffer The buffer where the read data would be stored (unused).
/// @param offset The offset from where the read operation should begin (unused).
/// @param size The number of bytes to read (unused).
/// @return Always returns 0, or -EINVAL if the file is NULL.
static ssize_t null_read(vfs_file_t *file, char *buffer, off_t offset, size_t size)
{
    // Check if the file is NULL.
    if (file == NULL) {
        pr_crit("null_read: Received NULL file\n");
        return -EINVAL; // Return an error if the file is NULL.
    }

    // Since this is a null device, no data is provided, and we always return 0.
    return 0;
}

/// @brief Retrieves the file status for a null device.
///
/// @param file A pointer to the vfs_file_t structure representing the file.
/// @param stat A pointer to the stat_t structure where the file status will be stored.
/// @return 0 on success, or -EINVAL if the file or stat is NULL.
static int null_fstat(vfs_file_t *file, stat_t *stat)
{
    // Check if the file or stat pointers are NULL.
    if (file == NULL || stat == NULL) {
        pr_crit("null_fstat: Received NULL file or stat pointer\n");
        return -EINVAL; // Return an error if either pointer is NULL.
    }

    // Log debug information.
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

int mem_devs_initialize(void)
{
    // Create the /dev/null device.
    struct memdev *devnull = null_device_create("/dev/null");
    if (!devnull) {
        pr_err("mem_devs_initialize: Failed to create /dev/null\n");
        return -ENODEV; // Return error if device creation fails.
    }

    // Register the null filesystem type.
    if (!vfs_register_filesystem(&null_file_system_type)) {
        pr_err("mem_devs_initialize: Failed to register NULL filesystem\n");
        return 1; // Return error if filesystem registration fails.
    }

    // Mount the /dev/null device.
    if (!vfs_register_superblock("null", "/dev/null", &null_file_system_type, devnull->file)) {
        pr_err("mem_devs_initialize: Failed to mount /dev/null\n");
        return 1; // Return error if mounting fails.
    }

    // Add the null device to the list of memory devices.
    if (add_device(devnull) < 0) {
        pr_err("mem_devs_initialize: Failed to register NULL filesystem\n");
        return 1; // Return error if filesystem registration fails.
    }

    return 0; // Return success.
}
