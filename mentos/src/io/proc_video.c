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
void print_rb(fs_rb_scancode_t *rb)
{
    if (!fs_rb_scancode_empty(rb)) {
        for (unsigned i = rb->read; (i < rb->write) && (i < rb->size); ++i) {
            pr_debug("%c", rb->buffer[i]);
        }
        pr_debug("\n");
    }
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
    fs_rb_scancode_t *rb = &process->keyboard_rb;

    // Pre-check the terminal flags.
    bool_t flg_icanon = bitmask_check(process->termios.c_lflag, ICANON) == ICANON;
    bool_t flg_echoe  = bitmask_check(process->termios.c_lflag, ECHOE) == ECHOE;
    bool_t flg_echo   = bitmask_check(process->termios.c_lflag, ECHO) == ECHO;
    bool_t flg_isig   = bitmask_check(process->termios.c_lflag, ISIG) == ISIG;

    // If we are in canonical mode and the last inserted element is a newline,
    // pop the buffer until it's empty.
    if (!fs_rb_scancode_empty(rb) && (!flg_icanon || (fs_rb_scancode_front(rb) == '\n'))) {
        // Return the newline character.
        *((char *)buf) = fs_rb_scancode_pop_back(rb) & 0x00FF;
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

    // Handle backspace.
    if (c == '\b') {
        // If ECHOE is off and ECHO is on, show the `^?` string.
        if (!flg_echoe && flg_echo) {
            video_puts("^?");
        }

        // If we are in canonical mode, pop the previous character.
        if (flg_icanon) {
            // Remove the last character from the buffer.
            fs_rb_scancode_pop_front(rb);

            // Optionally display the backspace character.
            if (flg_echoe) { video_putc(c); }
        } else {
            // Add the backspace character to the buffer.
            fs_rb_scancode_push_front(rb, c);

            // Return the backspace character.
            *((char *)buf) = fs_rb_scancode_pop_back(rb) & 0x00FF;

            // Indicate a character has been returned.
            return 1;
        }

        // No character returned for backspace handling.
        return 0;
    }

    // Handle delete key (0x7F).
    if (c == 0x7F) {
        if (flg_echo) {
            // Print escape sequence for delete.
            video_puts("^[[3~");
        }

        // Add the character to the buffer.
        fs_rb_scancode_push_front(rb, '\033');
        fs_rb_scancode_push_front(rb, '[');
        fs_rb_scancode_push_front(rb, '3');
        fs_rb_scancode_push_front(rb, '~');

        // Indicate no character was returned.
        return 0;
    }

    // Add the character to the buffer.
    fs_rb_scancode_push_front(rb, c);

    if (iscntrl(c)) {
        if ((c == 0x03) && (flg_isig)) {
            sys_kill(process->pid, SIGTERM);
        } else if ((c == 0x1A) && (flg_isig)) {
            sys_kill(process->pid, SIGSTOP);
        }

        if (isalpha('A' + (c - 1)) && (c != '\n') && (c != '\b') && (c != '\t')) {
            // If echo is activated, output the character to video.
            if (flg_echo) {
                video_putc('^');
                video_putc('A' + (c - 1));
            }

            fs_rb_scancode_push_front(rb, '\033');
            fs_rb_scancode_push_front(rb, '^');
            fs_rb_scancode_push_front(rb, 'A' + (c - 1));

            return 3;
        }
        // Echo the character.
        // if (flg_echo) {
        //     video_putc(c);
        // }
        // return 1;
    }

    // If we are NOT in canonical mode, send the character back to user immediately.
    if (!flg_icanon) {
        // Return the character.
        *((char *)buf) = fs_rb_scancode_pop_back(rb) & 0x00FF;
        // Indicate a character has been returned.
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
