/// @file proc_system.c
/// @brief Contains callbacks for procfs system files.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fs/procfs.h"
#include "version.h"
#include "process/process.h"
#include "string.h"
#include "stdio.h"
#include "sys/errno.h"
#include "io/debug.h"
#include "hardware/timer.h"

static ssize_t procs_do_uptime(char *buffer, size_t bufsize);

static ssize_t procs_do_version(char *buffer, size_t bufsize);

static ssize_t procs_do_mounts(char *buffer, size_t bufsize);

static ssize_t procs_do_cpuinfo(char *buffer, size_t bufsize);

static ssize_t procs_do_meminfo(char *buffer, size_t bufsize);

static ssize_t procs_do_stat(char *buffer, size_t bufsize);

static ssize_t procs_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    if (file == NULL)
        return -EFAULT;
    proc_dir_entry_t *entry = (proc_dir_entry_t *)file->device;
    if (entry == NULL)
        return -EFAULT;
    // Prepare a buffer.
    char buffer[BUFSIZ];
    memset(buffer, 0, BUFSIZ);
    // Call the specific function.
    int ret = 0;
    if (strcmp(entry->name, "uptime") == 0)
        ret = procs_do_uptime(buffer, BUFSIZ);
    else if (strcmp(entry->name, "version") == 0)
        ret = procs_do_version(buffer, BUFSIZ);
    else if (strcmp(entry->name, "mounts") == 0)
        ret = procs_do_mounts(buffer, BUFSIZ);
    else if (strcmp(entry->name, "cpuinfo") == 0)
        ret = procs_do_cpuinfo(buffer, BUFSIZ);
    else if (strcmp(entry->name, "meminfo") == 0)
        ret = procs_do_meminfo(buffer, BUFSIZ);
    else if (strcmp(entry->name, "stat") == 0)
        ret = procs_do_stat(buffer, BUFSIZ);
    // Perform read.
    ssize_t it = 0;
    if (ret == 0) {
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
    .mkdir_f = NULL,
    .rmdir_f = NULL,
    .stat_f  = NULL
};

/// Filesystem file operations.
static vfs_file_operations_t procs_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = procs_read,
    .write_f    = NULL,
    .lseek_f    = NULL,
    .stat_f     = NULL,
    .ioctl_f    = NULL,
    .getdents_f = NULL
};

int procs_module_init()
{
    proc_dir_entry_t *system_entry;
    // == /proc/uptime ========================================================
    if ((system_entry = proc_create_entry("uptime", NULL)) == NULL) {
        pr_err("Cannot create `/proc/uptime`.\n");
        return 1;
    }
    pr_debug("Created `/proc/uptime` (%p)\n", system_entry);
    // Set the specific operations.
    system_entry->sys_operations = &procs_sys_operations;
    system_entry->fs_operations  = &procs_fs_operations;

    // == /proc/version ========================================================
    if ((system_entry = proc_create_entry("version", NULL)) == NULL) {
        pr_err("Cannot create `/proc/version`.\n");
        return 1;
    }
    pr_debug("Created `/proc/version` (%p)\n", system_entry);
    // Set the specific operations.
    system_entry->sys_operations = &procs_sys_operations;
    system_entry->fs_operations  = &procs_fs_operations;

    // == /proc/mounts ========================================================
    if ((system_entry = proc_create_entry("mounts", NULL)) == NULL) {
        pr_err("Cannot create `/proc/mounts`.\n");
        return 1;
    }
    pr_debug("Created `/proc/mounts` (%p)\n", system_entry);
    // Set the specific operations.
    system_entry->sys_operations = &procs_sys_operations;
    system_entry->fs_operations  = &procs_fs_operations;

    // == /proc/cpuinfo ========================================================
    if ((system_entry = proc_create_entry("cpuinfo", NULL)) == NULL) {
        pr_err("Cannot create `/proc/cpuinfo`.\n");
        return 1;
    }
    pr_debug("Created `/proc/cpuinfo` (%p)\n", system_entry);
    // Set the specific operations.
    system_entry->sys_operations = &procs_sys_operations;
    system_entry->fs_operations  = &procs_fs_operations;

    // == /proc/meminfo ========================================================
    if ((system_entry = proc_create_entry("meminfo", NULL)) == NULL) {
        pr_err("Cannot create `/proc/meminfo`.\n");
        return 1;
    }
    pr_debug("Created `/proc/meminfo` (%p)\n", system_entry);
    // Set the specific operations.
    system_entry->sys_operations = &procs_sys_operations;
    system_entry->fs_operations  = &procs_fs_operations;

    // == /proc/stat ========================================================
    if ((system_entry = proc_create_entry("stat", NULL)) == NULL) {
        pr_err("Cannot create `/proc/stat`.\n");
        return 1;
    }
    pr_debug("Created `/proc/stat` (%p)\n", system_entry);
    // Set the specific operations.
    system_entry->sys_operations = &procs_sys_operations;
    system_entry->fs_operations  = &procs_fs_operations;
    return 0;
}

static ssize_t procs_do_uptime(char *buffer, size_t bufsize)
{
    sprintf(buffer, "%d", timer_get_seconds());
    return 0;
}

static ssize_t procs_do_version(char *buffer, size_t bufsize)
{
    sprintf(buffer,
            "%s version %s (site: %s) (email: %s)",
            OS_NAME,
            OS_VERSION,
            OS_SITEURL,
            OS_REF_EMAIL);
    return 0;
}

static ssize_t procs_do_mounts(char *buffer, size_t bufsize)
{
    return 0;
}

static ssize_t procs_do_cpuinfo(char *buffer, size_t bufsize)
{
    return 0;
}

static ssize_t procs_do_meminfo(char *buffer, size_t bufsize)
{
    double total_space = get_zone_total_space(GFP_KERNEL) +
                         get_zone_total_space(GFP_USER),
           free_space = get_zone_free_space(GFP_KERNEL) +
                        get_zone_free_space(GFP_USER),
           cached_space = get_zone_cached_space(GFP_KERNEL) +
                          get_zone_cached_space(GFP_USER),
           used_space = total_space - free_space;
    total_space /= (double)K;
    free_space /= (double)K;
    cached_space /= (double)K;
    used_space /= (double)K;
    sprintf(
        buffer,
        "MemTotal : %12.2f Kb\n"
        "MemFree  : %12.2f Kb\n"
        "MemUsed  : %12.2f Kb\n"
        "Cached   : %12.2f Kb\n",
        total_space, free_space, used_space, cached_space);
    return 0;
}

static ssize_t procs_do_stat(char *buffer, size_t bufsize)
{
    return 0;
}
