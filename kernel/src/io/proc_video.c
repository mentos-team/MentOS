/// @file proc_video.c
/// @brief Contains callbacks for procv system files.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[PROCV ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "bits/ioctls.h"
#include "bits/termios-struct.h"
#include "ctype.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/keyboard/keymap.h"
#include "errno.h"
#include "fcntl.h"
#include "fs/procfs.h"
#include "fs/vfs.h"
#include "io/video.h"
#include "process/scheduler.h"
#include "sys/bitops.h"

#define DISPLAY_CHAR(c) (iscntrl(c) ? ' ' : (c))
#define ERASE_CHAR()      \
    do {                  \
        video_putc('\b'); \
        video_putc(' ');  \
        video_putc('\b'); \
    } while (0)

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
    rb_keybuffer_t *rb   = &process->keyboard_rb;

    // Pre-check the terminal flags.
    bool_t flg_icanon  = (process->termios.c_lflag & ICANON) == ICANON;
    bool_t flg_echoe   = (process->termios.c_lflag & ECHOE) == ECHOE;
    bool_t flg_echonl  = (process->termios.c_lflag & ECHONL) == ECHONL;
    bool_t flg_echoctl = (process->termios.c_lflag & ECHOCTL) == ECHOCTL;
    bool_t flg_echo    = (process->termios.c_lflag & ECHO) == ECHO;
    bool_t flg_isig    = (process->termios.c_lflag & ISIG) == ISIG;
    // Not implemented yet.
    bool_t flg_echok   = (process->termios.c_lflag & ECHOK) == ECHOK;
    bool_t flg_echoke  = (process->termios.c_lflag & ECHOKE) == ECHOKE;
    bool_t flg_noflsh  = (process->termios.c_lflag & NOFLSH) == NOFLSH;
    bool_t flg_tostop  = (process->termios.c_lflag & TOSTOP) == TOSTOP;
    bool_t flg_iexten  = (process->termios.c_lflag & IEXTEN) == IEXTEN;

    // If we are in canonical mode and the last inserted element is a newline,
    // pop the buffer until it's empty.
    if (!rb_keybuffer_is_empty(rb) && (!flg_icanon || (rb_keybuffer_peek_front(rb) == '\n'))) {
        // Return the newline character.
        *(buf) = rb_keybuffer_pop_back(rb) & 0x00FF;
        pr_debug("POP BUFFER  [%c](%d)\n", DISPLAY_CHAR(*buf), (*buf));
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

    // Handle special characters.
    switch (c) {
    case '\t':
        if (flg_echo) {
            for (int i = 0; i < 4; ++i) {
                video_putc(' ');
            }
        }
        *(buf) = c;
        pr_debug("RETURN      [%c](%d)\n", DISPLAY_CHAR(*buf), *buf);
        return 1;

    case '\n':
        if (flg_echo || (flg_icanon && flg_echonl)) {
            video_putc(c);
        }
        if (flg_icanon) {
            rb_keybuffer_push_front(rb, c);
            pr_debug("PUSH BUFFER [%c](%d) (Trigger consumption of the buffer)\n", DISPLAY_CHAR(c), c);
            return 0;
        }
        *(buf) = c;
        pr_debug("RETURN      [%c](%d)\n", DISPLAY_CHAR(*buf), *buf);
        return 1;

    case 0x15: // ^U (KILL)
        if (flg_icanon) {
            // Flush entire line buffer
            while (!rb_keybuffer_is_empty(rb)) {
                rb_keybuffer_pop_front(rb);
                if (flg_echoke) {
                    // Visually erase characters if ECHOKE is set
                    ERASE_CHAR();
                }
            }

            if (flg_echok) {
                // Print newline after KILL if ECHOK is set
                video_putc('\n');
            }

            return 0;
        }

    case '\b':
    case 127:
        if (flg_icanon) {
            // Canonical mode: erase last character in buffer
            if (!rb_keybuffer_is_empty(rb)) {
                rb_keybuffer_pop_front(rb);
                if (flg_echoe) {
                    // Visually erase the character.
                    ERASE_CHAR();
                } else if (flg_echo) {
                    // Fallback echo for ECHO without ECHOE
                    video_puts("^?");
                }
            }
            return 0; // No char returned
        }

        // Non-canonical: treat as input
        if (flg_echo) {
            video_putc(c);
        }
        rb_keybuffer_push_front(rb, c);
        *(buf) = rb_keybuffer_pop_back(rb) & 0x00FF;
        pr_debug("RETURN      [%c](%d)\n", DISPLAY_CHAR(*buf), (*buf));
        return 1;

    default:
        if (iscntrl(c)) {
            // Handle signal-generating control characters
            if (flg_isig) {
                if (c == 0x03) { // Ctrl+C
                    sys_kill(process->pid, SIGTERM);
                    return 0;
                }
                if (c == 0x1A) { // Ctrl+Z
                    sys_kill(process->pid, SIGSTOP);
                    return 0;
                }
            }

            // Echo control characters as ^X if ECHOCTL is set
            if (flg_echo && flg_echoctl) {
                if (c == '\n') {
                    if (flg_echo || (flg_icanon && flg_echonl)) {
                        video_putc('\n');
                    }
                } else {
                    video_putc('^');
                    video_putc(c + '@'); // e.g., Ctrl+C → ^C
                }
            } else if (flg_echo && c == '\n') {
                // ECHOCTL not set: echo newline only if allowed
                if (flg_echo || (flg_icanon && flg_echonl)) {
                    video_putc('\n');
                }
            }
        } else if (flg_echo) {
            // Printable character
            video_putc(c);
        }
        break;
    }
    // If in canonical mode, push the character to the ring buffer.
    if (flg_icanon) {
        // Add the character to the ring buffer.
        rb_keybuffer_push_front(rb, c);
        pr_debug("PUSH BUFFER [%c](%d)\n", (iscntrl(c) ? ' ' : c), c);
        return 0;
    }
    // If NOT in canonical mode, return the character.
    *(buf) = c & 0x00FF;
    pr_debug("RETURN      [%c](%d)\n", DISPLAY_CHAR(*buf), (*buf));
    return 1;
}

/// @brief Writes data to the video output by sending each character from the buffer to the video output.
///
/// @param file Pointer to the file structure (unused).
/// @param buf Pointer to the buffer containing the data to write.
/// @param offset Offset within the file (unused).
/// @param nbyte Number of bytes to write from the buffer.
/// @return ssize_t The number of bytes written.
static ssize_t procv_write(vfs_file_t *file, const void *buf, off_t offset, size_t nbyte)
{
    for (size_t i = 0; i < nbyte; ++i) {
        video_putc(((char *)buf)[i]);
    }
    return nbyte;
}

/// @brief Handles ioctl requests for the process, including terminal settings (TCGETS and TCSETS).
///
/// @param file Pointer to the file structure (unused).
/// @param request The ioctl request code (e.g., TCGETS, TCSETS).
/// @param data Pointer to the data structure for the ioctl request (e.g., termios).
/// @return int Returns 0 on success.
static long procv_ioctl(vfs_file_t *file, unsigned int request, unsigned long data)
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
