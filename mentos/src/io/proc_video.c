/// @file proc_video.c
/// @brief Contains callbacks for procv system files.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[PROCV ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_DEBUG

#include "bits/termios-struct.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/keyboard/keymap.h"
#include "fs/procfs.h"
#include "bits/ioctls.h"
#include "sys/bitops.h"
#include "io/video.h"
#include "io/debug.h"
#include "sys/errno.h"
#include "fcntl.h"
#include "fs/vfs.h"
#include "ctype.h"
#include "process/scheduler.h"

static ssize_t procv_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    if (buf == NULL) {
        return -1;
    }
    // Read the character from the keyboard.
    int c = keyboard_getc();
    if (c < 0)
        return 0;
    if (c == KEY_PAGE_UP) {
        video_shift_one_page_down();
        return 0;
    } else if (c == KEY_PAGE_DOWN) {
        video_shift_one_page_up();
        return 0;
    } else {
        // Get the currently running process.
        task_struct *process = scheduler_get_current_process();
        // Return the character.
        *((char *)buf) = c & 0x00FF;
        if (bitmask_check(process->termios.c_lflag, ECHO)) {
            if ((c == '\b') &&
                (!bitmask_check(process->termios.c_lflag, ICANON) || !bitmask_check(process->termios.c_lflag, ECHOE))) {
                return 1;
            }
            // Echo the character to video.
            video_putc(c & 0x00FF);
        }
    }
    return 1;
}

static ssize_t procv_write(vfs_file_t *file, const void *buf, off_t offset, size_t nbyte)
{
    for (size_t i = 0; i < nbyte; ++i) {
        video_putc(((char *)buf)[i]);
    }
    return nbyte;
}

static int procv_fstat(vfs_file_t *file, stat_t *stat)
{
    return -ENOSYS;
}

static int procv_ioctl(vfs_file_t *file, int request, void *data)
{
    task_struct *process = scheduler_get_current_process();
    switch (request) {
    case TCGETS:
        *((termios_t *)data) = process->termios;
        break;
    case TCSETS:
        process->termios = *((termios_t *)data);
        break;
    default:
        break;
    }
    return 0;
}

/// Filesystem general operations.
static vfs_sys_operations_t procv_sys_operations = {
    .mkdir_f = NULL,
    .rmdir_f = NULL,
    .stat_f  = NULL
};

/// Filesystem file operations.
static vfs_file_operations_t procv_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = procv_read,
    .write_f    = procv_write,
    .lseek_f    = NULL,
    .stat_f     = procv_fstat,
    .ioctl_f    = procv_ioctl,
    .getdents_f = NULL
};

int procv_module_init()
{
    proc_dir_entry_t *video = proc_create_entry("video", NULL);
    if (video == NULL) {
        pr_err("Cannot create `/proc/video`.\n");
        return 1;
    }
    pr_debug("Created `/proc/video` (%p)\n", video);
    // Set the specific operations.
    video->sys_operations = &procv_sys_operations;
    video->fs_operations  = &procv_fs_operations;
    return 0;
}