/// @file proc_video.c
/// @brief Contains callbacks for procv system files.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[PROCV ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "bits/ioctls.h"
#include "bits/termios-struct.h"
#include "ctype.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/keyboard/keymap.h"
#include "fcntl.h"
#include "fs/procfs.h"
#include "fs/vfs.h"
#include "io/video.h"
#include "process/scheduler.h"
#include "sys/bitops.h"
#include "sys/errno.h"

/// @brief Prints the ring-buffer.
/// @param rb the ring-buffer to print.
void rb_keybuffer_print(rb_keybuffer_t *rb)
{
    pr_debug("(%u)[ ", rb->count);
    for (unsigned i = 0; i < rb->count; ++i) {
        pr_debug("%d ", rb_keybuffer_get(rb, i));
    }
    pr_debug("]\n");
}

/// @brief Read function for the proc video system.
/// @param file The file.
/// @param buf Buffer where the read content must be placed.
/// @param offset Offset from which we start reading from the file.
/// @param nbyte The number of bytes to read.
/// @return The number of red bytes.
static ssize_t procv_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    // Stop if the buffer is invalid.
    if (buf == NULL) {
        return -1;
    }

    // Get the currently running process.
    task_struct *process = scheduler_get_current_process();
    // Get a pointer to its keyboard ring buffer.
    rb_keybuffer_t *rb = &process->keyboard_rb;

    // Pre-check the terminal flags.
    bool_t flg_icanon = bitmask_check(process->termios.c_lflag, ICANON) == ICANON;
    bool_t flg_echoe  = bitmask_check(process->termios.c_lflag, ECHOE) == ECHOE;
    bool_t flg_echo   = bitmask_check(process->termios.c_lflag, ECHO) == ECHO;
    bool_t flg_isig   = bitmask_check(process->termios.c_lflag, ISIG) == ISIG;

    // If we are in canonical mode and the last inserted element is a newline,
    // pop the buffer until it's empty.
    if (!rb_keybuffer_is_empty(rb) && (!flg_icanon || (rb_keybuffer_peek_front(rb) == '\n'))) {
        // Return the newline character.
        rb_keybuffer_print(rb);
        *((char *)buf) = rb_keybuffer_pop_back(rb) & 0x00FF;
        // Indicate a character has been returned.
        return 1;
    }

    // Once we have dealt with canonical mode, get the character.
    int c = keyboard_pop_back();

    // Check that it's a valid character.
    if (c < 0) {
        return 0; // No valid character received.
    }

    // Keep only the character, not the scancode.
    c &= 0x00FF;

    if (iscntrl(c))
        pr_debug("[ ](%d)\n", c);
    else
        pr_debug("[%c](%d)\n", c, c);

    // Handle special characters.
    switch (c) {
    case '\n':
    case '\t':
        // Optionally display the character if echo is enabled.
        if (flg_echo) {
            video_putc(c);
        }
        // Return the character.
        *((char *)buf) = c;
        return 1;

    case '\b':
        // Handle backspace.
        if (!flg_echoe && flg_echo) {
            video_puts("^?");
        }
        if (flg_icanon) {
            // Canonical mode: Remove the last character from the buffer.
            if (!rb_keybuffer_is_empty(rb)) {
                rb_keybuffer_pop_front(rb);
                if (flg_echoe) {
                    video_putc(c);
                }
            }
            return 0; // No character returned for backspace.
        } else {
            // Non-canonical mode: Add backspace to the buffer and return it.
            rb_keybuffer_push_front(rb, c);
            *((char *)buf) = rb_keybuffer_pop_back(rb) & 0x00FF;
            return 1;
        }

    case 127: // Delete key.
        // Optionally display the character if echo is enabled.
        if (flg_echo) {
            video_putc(c);
        }
        // Return the character.
        *((char *)buf) = c;
        return 1;

    default:
        if (iscntrl(c)) {
            // Handle control characters in both canonical and non-canonical modes.
            if (flg_isig) {
                if (c == 0x03) {
                    // Send SIGTERM on Ctrl+C.
                    sys_kill(process->pid, SIGTERM);
                    return 0;
                } else if (c == 0x1A) {
                    // Send SIGSTOP on Ctrl+Z.
                    sys_kill(process->pid, SIGSTOP);
                    return 0;
                }
            }
            if (isalpha('A' + (c - 1))) {
                // Display control character representation (e.g., ^A).
                if (flg_echo) {
                    video_putc('^');
                    video_putc('A' + (c - 1));
                }
            }
        }
    }

    // Add the character to the buffer.
    rb_keybuffer_push_front(rb, c);

    // If not in canonical mode, return the character immediately.
    if (!flg_icanon) {
        *((char *)buf) = rb_keybuffer_pop_back(rb) & 0x00FF;
        return 1;
    }

    // Default return indicating no character was processed.
    return 0;
}

static ssize_t procv_write(vfs_file_t *file, const void *buf, off_t offset, size_t nbyte)
{
    for (size_t i = 0; i < nbyte; ++i) {
        video_putc(((char *)buf)[i]);
    }
    return nbyte;
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
    .mkdir_f   = NULL,
    .rmdir_f   = NULL,
    .stat_f    = NULL,
    .creat_f   = NULL,
    .symlink_f = NULL,
};

/// Filesystem file operations.
static vfs_file_operations_t procv_fs_operations = {
    .open_f     = NULL,
    .unlink_f   = NULL,
    .close_f    = NULL,
    .read_f     = procv_read,
    .write_f    = procv_write,
    .lseek_f    = NULL,
    .stat_f     = NULL,
    .ioctl_f    = procv_ioctl,
    .getdents_f = NULL,
    .readlink_f = NULL,
};

int procv_module_init(void)
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

    if (proc_entry_set_mask(video, 0666) < 0) {
        pr_err("Cannot set mask for `/proc/video`.\n");
        return 1;
    }

    return 0;
}
