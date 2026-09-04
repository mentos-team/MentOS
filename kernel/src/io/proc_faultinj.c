/// @file proc_faultinj.c
/// @brief `/proc/faultinj`: deliberate failure of block-device transfers.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[FAULTI]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "io/fault_injection.h"

#ifdef ENABLE_ATA_FAULT_INJECTION

#include "fs/procfs.h"
#include "stdio.h"
#include "string.h"
#include "errno.h"

/// @brief How many transfers are still to be failed, and how many have been.
/// @details A single-processor, non-preemptible kernel reaches the block layer
///          one request at a time, so plain counters need no locking here. If
///          that stops being true this state has to be revisited along with
///          everything else that assumes it.
static struct {
    unsigned reads_armed;     ///< Reads still to be failed.
    unsigned writes_armed;    ///< Writes still to be failed.
    unsigned reads_injected;  ///< Reads failed since the last arming.
    unsigned writes_injected; ///< Writes failed since the last arming.
} __faultinj;

int ata_fault_inject_read(void)
{
    if (__faultinj.reads_armed == 0) {
        return 0;
    }
    --__faultinj.reads_armed;
    ++__faultinj.reads_injected;
    // -EIO rather than -EBUSY: a caller that retries transient statuses must
    // still see a hard failure propagate, and -EIO is the one the retry does
    // not swallow.
    pr_notice("Injecting a read failure (%u left armed).\n", __faultinj.reads_armed);
    return -EIO;
}

int ata_fault_inject_write(void)
{
    if (__faultinj.writes_armed == 0) {
        return 0;
    }
    --__faultinj.writes_armed;
    ++__faultinj.writes_injected;
    pr_notice("Injecting a write failure (%u left armed).\n", __faultinj.writes_armed);
    return -EIO;
}

void ata_fault_inject_arm(unsigned reads, unsigned writes)
{
    __faultinj.reads_armed     = reads;
    __faultinj.writes_armed    = writes;
    __faultinj.reads_injected  = 0;
    __faultinj.writes_injected = 0;
    pr_notice("Armed for %u read and %u write failures.\n", reads, writes);
}

/// @brief Reports what is armed and what has fired.
/// @param file the file to read from.
/// @param buf the buffer where the data is placed.
/// @param offset the offset to read from.
/// @param nbyte the size of the buffer.
/// @return the number of bytes placed in the buffer.
static ssize_t procfaultinj_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    char content[128];
    int written = sprintf(
        content, "reads_armed %u\nwrites_armed %u\nreads_injected %u\nwrites_injected %u\n", __faultinj.reads_armed,
        __faultinj.writes_armed, __faultinj.reads_injected, __faultinj.writes_injected);
    if (written < 0) {
        return -EIO;
    }
    // A read past the end is end of file, not an error.
    if (offset >= written) {
        return 0;
    }
    size_t available = (size_t)(written - offset);
    size_t to_copy   = (nbyte < available) ? nbyte : available;
    memcpy(buf, content + offset, to_copy);
    return (ssize_t)to_copy;
}

/// @brief Arms the injection from a written command.
/// @param file the file to write to.
/// @param buf the buffer holding the command.
/// @param offset ignored: a write is a command, not a position in a file.
/// @param nbyte the length of the command.
/// @return the number of bytes consumed, or a negative errno.
/// @details Accepts `read <n>`, `write <n>` and `off`. Anything else is
///          rejected with -EINVAL rather than partially applied, so a
///          mistyped command cannot leave the injection in a state the test
///          did not ask for.
static ssize_t procfaultinj_write(vfs_file_t *file, const void *buf, off_t offset, size_t nbyte)
{
    char command[64];
    if (nbyte == 0) {
        return 0;
    }
    if (nbyte >= sizeof(command)) {
        return -EINVAL;
    }
    memcpy(command, buf, nbyte);
    command[nbyte] = 0;
    // Drop a trailing newline, so `echo` works without `-n`.
    if ((nbyte > 0) && (command[nbyte - 1] == '\n')) {
        command[nbyte - 1] = 0;
    }

    if (strcmp(command, "off") == 0) {
        ata_fault_inject_arm(0, 0);
        return (ssize_t)nbyte;
    }
    // The count is whatever follows the keyword and a single space.
    unsigned count = 0;
    const char *rest;
    if (strncmp(command, "read ", 5) == 0) {
        rest = command + 5;
    } else if (strncmp(command, "write ", 6) == 0) {
        rest = command + 6;
    } else {
        pr_err("Unrecognized command `%s`.\n", command);
        return -EINVAL;
    }
    // Parse by hand: the count has to be digits and nothing else, so that a
    // typo is refused instead of silently becoming zero.
    if (*rest == 0) {
        return -EINVAL;
    }
    for (const char *c = rest; *c != 0; ++c) {
        if ((*c < '0') || (*c > '9')) {
            pr_err("Not a count: `%s`.\n", rest);
            return -EINVAL;
        }
        count = (count * 10U) + (unsigned)(*c - '0');
    }
    if (command[0] == 'r') {
        ata_fault_inject_arm(count, __faultinj.writes_armed);
    } else {
        ata_fault_inject_arm(__faultinj.reads_armed, count);
    }
    return (ssize_t)nbyte;
}

/// Filesystem file operations for the entry.
static vfs_file_operations_t procfaultinj_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = procfaultinj_read,
    .write_f    = procfaultinj_write,
    .lseek_f    = NULL,
    .stat_f     = NULL,
    .ioctl_f    = NULL,
    .getdents_f = NULL,
    .readlink_f = NULL,
};

int procfaultinj_module_init(void)
{
    proc_dir_entry_t *entry = proc_create_entry("faultinj", NULL);
    if (entry == NULL) {
        pr_err("Cannot create `/proc/faultinj`.\n");
        return 1;
    }
    entry->fs_operations = &procfaultinj_fs_operations;
    // Writable, and only by root: arming this makes the filesystem return
    // errors on demand, which is a way to lose data if it is left armed.
    if (proc_entry_set_mask(entry, 0600) < 0) {
        pr_err("Cannot set mask of `/proc/faultinj`.\n");
        return 1;
    }
    pr_notice("Fault injection is available at `/proc/faultinj`.\n");
    return 0;
}

#endif // ENABLE_ATA_FAULT_INJECTION
