/// @file sync.c
/// @brief Filesystem synchronization syscalls implementation.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// @details These three calls have nothing to flush, and that is the correct
/// behaviour rather than an unfinished one. There is no buffer cache in this
/// kernel: `ext2_write_block` hands the block straight to `vfs_write` on the
/// block device, which reaches the ATA driver, which writes. Nothing sits
/// between a write and the disk, so by the time a write returns there is no
/// state left for a sync to push out.
///
/// They therefore succeed without doing work. What they still owe the caller
/// is the error contract: a descriptor that is not open is `-EBADF`, not a
/// silent success, because a program that checks the return of `syncfs` is
/// entitled to learn that it passed a descriptor that does not exist.
///
/// The day a write-back cache appears, these are the functions that have to
/// grow a body, and nothing else has to move (#408).

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[SYNC  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "errno.h"
#include "fs/vfs.h"
#include "process/scheduler.h"

/// @brief Tells whether a descriptor is one the calling task has open.
/// @param fd the descriptor to check.
/// @return 0 when it is, -EBADF when it is not.
/// @details A non-negative number is not a descriptor: the slot has to be
///          inside the task's table and has to hold a file.
static int __check_fd(int fd)
{
    task_struct *task = scheduler_get_current_process();
    if (task == NULL) {
        return -EBADF;
    }
    if ((fd < 0) || (fd >= task->max_fd)) {
        return -EBADF;
    }
    if (task->fd_list[fd].file_struct == NULL) {
        return -EBADF;
    }
    return 0;
}

/// @brief Synchronize all filesystems to persistent storage.
/// @return 0, always.
/// @details Writes reach the disk before they return, so there is nothing
///          outstanding to wait for. See the note at the top of this file.
long sys_sync(void)
{
    pr_debug("sys_sync()\n");
    return 0;
}

/// @brief Synchronize the filesystem holding an open file.
/// @param fd file descriptor of an open file on the target filesystem.
/// @return 0 on success, -EBADF when `fd` is not open in the calling task.
/// @details Writes reach the disk before they return, so the work is already
///          done; the descriptor is still validated, because reporting
///          success for a descriptor the caller does not hold would hide the
///          caller's own bug.
long sys_syncfs(int fd)
{
    pr_debug("sys_syncfs(%d)\n", fd);
    int result = __check_fd(fd);
    if (result < 0) {
        return result;
    }
    return 0;
}

/// @brief Synchronize a range of bytes in a file to persistent storage.
/// @param fd file descriptor of the file to sync.
/// @param offset starting byte offset in the file.
/// @param nbytes number of bytes to sync, 0 meaning to the end of the file.
///        Both offsets are `off_t`: the dispatcher forwards five argument
///        registers, and two 64-bit ones would not fit (#408).
/// @param flags sync behaviour flags (SYNC_FILE_RANGE_* constants).
/// @return 0 on success, -EBADF when `fd` is not open in the calling task,
///         -EINVAL when the range is negative.
/// @details Writes reach the disk before they return, so the range is
///          already persistent; the arguments are still checked, so that a
///          caller passing a nonsensical range hears about it here rather
///          than the day this call grows a body.
long sys_sync_file_range(int fd, off_t offset, off_t nbytes, unsigned int flags)
{
    pr_debug("sys_sync_file_range(%d, %ld, %ld, 0x%x)\n", fd, offset, nbytes, flags);
    int result = __check_fd(fd);
    if (result < 0) {
        return result;
    }
    if ((offset < 0) || (nbytes < 0)) {
        return -EINVAL;
    }
    return 0;
}
