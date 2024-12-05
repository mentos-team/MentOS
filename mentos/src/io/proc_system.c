/// @file proc_system.c
/// @brief Contains callbacks for procfs system files.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fs/procfs.h"
#include "hardware/timer.h"
#include "io/debug.h"
#include "process/process.h"
#include "stdio.h"
#include "string.h"
#include "errno.h"
#include "version.h"

static ssize_t procs_do_uptime(char *buffer, size_t bufsize);

static ssize_t procs_do_version(char *buffer, size_t bufsize);

static ssize_t procs_do_mounts(char *buffer, size_t bufsize);

static ssize_t procs_do_cpuinfo(char *buffer, size_t bufsize);

static ssize_t procs_do_meminfo(char *buffer, size_t bufsize);

static ssize_t procs_do_stat(char *buffer, size_t bufsize);

/// @brief Read function for the proc system.
/// @param file The file.
/// @param buf Buffer where the read content must be placed.
/// @param offset Offset from which we start reading from the file.
/// @param nbyte The number of bytes to read.
/// @return The number of red bytes.
static ssize_t __procs_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    if (!file) {
        pr_err("We received a NULL file pointer.\n");
        return -EFAULT;
    }
    proc_dir_entry_t *entry = (proc_dir_entry_t *)file->device;
    if (entry == NULL) {
        pr_err("The file is not a valid proc entry.\n");
        return -EFAULT;
    }
    // Prepare a buffer.
    char buffer[BUFSIZ];
    memset(buffer, 0, BUFSIZ);
    // Call the specific function.
    int ret = 0;
    if (strcmp(entry->name, "uptime") == 0) {
        ret = procs_do_uptime(buffer, BUFSIZ);
    } else if (strcmp(entry->name, "version") == 0) {
        ret = procs_do_version(buffer, BUFSIZ);
    } else if (strcmp(entry->name, "mounts") == 0) {
        ret = procs_do_mounts(buffer, BUFSIZ);
    } else if (strcmp(entry->name, "cpuinfo") == 0) {
        ret = procs_do_cpuinfo(buffer, BUFSIZ);
    } else if (strcmp(entry->name, "meminfo") == 0) {
        ret = procs_do_meminfo(buffer, BUFSIZ);
    } else if (strcmp(entry->name, "stat") == 0) {
        ret = procs_do_stat(buffer, BUFSIZ);
    }
    // Perform read.
    ssize_t it = 0;
    if (ret >= 0) {
        size_t name_len = strlen(buffer);
        size_t read_pos = offset;
        if (read_pos < name_len) {
            while ((it < nbyte) && (read_pos < name_len)) {
                *buf++ = buffer[read_pos];
                ++read_pos;
                ++it;
            }
        }
    }
    return it;
}

/// Filesystem general operations.
static vfs_sys_operations_t procs_sys_operations = {
    .mkdir_f   = NULL,
    .rmdir_f   = NULL,
    .stat_f    = NULL,
    .creat_f   = NULL,
    .symlink_f = NULL,
};

/// Filesystem file operations.
static vfs_file_operations_t procs_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = __procs_read,
    .write_f    = NULL,
    .lseek_f    = NULL,
    .stat_f     = NULL,
    .ioctl_f    = NULL,
    .getdents_f = NULL,
    .readlink_f = NULL,
};

int procs_module_init(void)
{
    proc_dir_entry_t *system_entry;
    char *entry_names[] = { "uptime", "version", "mounts", "cpuinfo", "meminfo", "stat" };
    for (int i = 0; i < count_of(entry_names); i++) {
        char *entry_name = entry_names[i];
        if ((system_entry = proc_create_entry(entry_name, NULL)) == NULL) {
            pr_err("Cannot create `/proc/%s`.\n", entry_name);
            return 1;
        }
        pr_debug("Created `/proc/%s` (%p)\n", entry_name, system_entry);
        // Set the specific operations.
        system_entry->sys_operations = &procs_sys_operations;
        system_entry->fs_operations  = &procs_fs_operations;
        if (proc_entry_set_mask(system_entry, 0444) < 0) {
            pr_err("Cannot set mask of `/proc/%s`.\n", entry_name);
            return 1;
        }
    }

    return 0;
}

/// @brief Write the uptime inside the buffer.
/// @param buffer the buffer.
/// @param bufsize the buffer size.
/// @return the amount we wrote.
static ssize_t procs_do_uptime(char *buffer, size_t bufsize)
{
    return sprintf(buffer, "%d", timer_get_seconds());
}

/// @brief Write the version inside the buffer.
/// @param buffer the buffer.
/// @param bufsize the buffer size.
/// @return the amount we wrote.
static ssize_t procs_do_version(char *buffer, size_t bufsize)
{
    return sprintf(buffer,
                   "%s version %s (site: %s) (email: %s)",
                   OS_NAME,
                   OS_VERSION,
                   OS_SITEURL,
                   OS_REF_EMAIL);
}

/// @brief Write the list of mount points inside the buffer.
/// @param buffer the buffer.
/// @param bufsize the buffer size.
/// @return the amount we wrote.
static ssize_t procs_do_mounts(char *buffer, size_t bufsize)
{
    return 0;
}

/// @brief Write the cpu information inside the buffer.
/// @param buffer the buffer.
/// @param bufsize the buffer size.
/// @return the amount we wrote.
static ssize_t procs_do_cpuinfo(char *buffer, size_t bufsize)
{
    return 0;
}

/// @brief Write the memory information inside the buffer.
/// @param buffer the buffer.
/// @param bufsize the buffer size.
/// @return the amount we wrote.
static ssize_t procs_do_meminfo(char *buffer, size_t bufsize)
{
    double total_space = get_zone_total_space(GFP_KERNEL) +
                         get_zone_total_space(GFP_HIGHUSER),
           free_space = get_zone_free_space(GFP_KERNEL) +
                        get_zone_free_space(GFP_HIGHUSER),
           cached_space = get_zone_cached_space(GFP_KERNEL) +
                          get_zone_cached_space(GFP_HIGHUSER),
           used_space = total_space - free_space;
    // Buddy system status strings.
    char kernel_buddy_status[512] = { 0 };
    char user_buddy_status[512]   = { 0 };
    get_zone_buddy_system_status(GFP_KERNEL, kernel_buddy_status, sizeof(kernel_buddy_status));
    get_zone_buddy_system_status(GFP_HIGHUSER, user_buddy_status, sizeof(user_buddy_status));
    // Format and return the information for the buffer.
    return snprintf(
        buffer,
        bufsize,
        "MemTotal       : %12.2f Kb\n"
        "MemFree        : %12.2f Kb\n"
        "MemUsed        : %12.2f Kb\n"
        "Cached         : %12.2f Kb\n"
        "Kernel Zone    : %s\n"
        "User Zone      : %s\n",
        total_space / (double)K,
        free_space / (double)K,
        used_space / (double)K,
        cached_space / (double)K,
        kernel_buddy_status,
        user_buddy_status);
}

/// @brief Write the process statistics inside the buffer.
/// @param buffer the buffer.
/// @param bufsize the buffer size.
/// @return the amount we wrote.
static ssize_t procs_do_stat(char *buffer, size_t bufsize)
{
    return 0;
}
