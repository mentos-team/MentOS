/// @file   debug.c
/// @brief  Debugging primitives.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "io/debug.h"
#include "io/ansi_colors.h"
#include "io/port_io.h"
#include "math.h"
#include "stdio.h"
#include "string.h"
#include "sys/bitops.h"

/// Serial port for QEMU.
#define SERIAL_COM1 (0x03F8)
/// Determines the log level.
static int max_log_level = LOGLEVEL_DEBUG;

void dbg_putchar(char c)
{
    outportb(SERIAL_COM1, (unsigned char)c);
}

void dbg_puts(const char *s)
{
    while ((*s) != 0) {
        dbg_putchar(*s++);
    }
}

static inline void __debug_print_header(const char *file, const char *fun, int line, short log_level, char *header)
{
    // "EMERG  ", "ALERT  ", "CRIT   ", "ERR    ", "WARNING", "NOTICE ", "INFO   ", "DEBUG  ", "DEFAULT",
    static const char *log_level_label[] = { " EM ", " AL ", " CR ", " ER ", " WR ", " NT ", " IN ", " DB ", " DF " };
    static const char *log_level_color[] = {
        FG_RED_BRIGHT,    // "EMERG  "
        FG_RED_BRIGHT,    // "ALERT  "
        FG_RED,           // "CRIT   "
        FG_RED,           // "ERR    "
        FG_YELLOW_BRIGHT, // "WARNING"
        FG_RESET,         // "NOTICE "
        FG_CYAN,          // "INFO   "
        FG_YELLOW,        // "DEBUG  "
        FG_RESET          // "DEFAULT"
    };
    static char tmp_prefix[BUFSIZ], final_prefix[BUFSIZ];
    // Check the log level.
    if ((log_level < LOGLEVEL_EMERG) || (log_level > LOGLEVEL_DEBUG)) {
        // Set it to default.
        log_level = 8;
    }
    // Set the color.
    dbg_puts(log_level_color[log_level]);
    dbg_putchar('[');
    // Set the label.
    dbg_puts(log_level_label[log_level]);
    dbg_putchar('|');
    // Print the file and line.
    sprintf(tmp_prefix, "%s:%d", file, line);
    // Print the message.
    sprintf(final_prefix, " %-20s ", tmp_prefix);
    // Print the actual message.
    dbg_puts(final_prefix);
#if 0
    dbg_putchar('|');
    sprintf(final_prefix, " %-25s ]", fun);
    dbg_puts(final_prefix);
#else
    dbg_putchar(']');
#endif
    dbg_putchar(' ');
    if (header) {
        dbg_puts(header);
        dbg_putchar(' ');
    }
}

void set_log_level(int level)
{
    if ((level >= LOGLEVEL_EMERG) && (level <= LOGLEVEL_DEBUG)) {
        max_log_level = level;
    }
}

int get_log_level(void)
{
    return max_log_level;
}

void dbg_printf(const char *file, const char *fun, int line, char *header, short log_level, const char *format, ...)
{
    // Define a buffer for the formatted string.
    static char formatted[BUFSIZ];
    static short new_line = 1;

    // Stage 1: FORMAT
    if (strlen(format) >= BUFSIZ) {
        return;
    }

    // Start variabile argument's list.
    va_list ap;
    va_start(ap, format);
    // Format the message.
    vsprintf(formatted, format, ap);
    // End the list of arguments.
    va_end(ap);

    // Stage 2: SEND
    if (new_line) {
        __debug_print_header(file, fun, line, log_level, header);
        new_line = 0;
    }
    for (int it = 0; (formatted[it] != 0) && (it < BUFSIZ); ++it) {
        dbg_putchar(formatted[it]);
        if (formatted[it] != '\n') {
            continue;
        }
        if ((it + 1) >= BUFSIZ) {
            continue;
        }
        if (formatted[it + 1] == 0) {
            new_line = 1;
        } else {
            __debug_print_header(file, fun, line, log_level, header);
        }
    }
}

const char *to_human_size(unsigned long bytes)
{
    static char output[200];
    const char *suffix[] = { "B", "KB", "MB", "GB", "TB" };
    char length          = sizeof(suffix) / sizeof(suffix[0]);
    int i                = 0;
    double dblBytes      = bytes;
    if (bytes > 1024) {
        for (i = 0; (bytes / 1024) > 0 && i < length - 1; i++, bytes /= 1024) {
            dblBytes = bytes / 1024.0;
        }
    }
    sprintf(output, "%.02lf %2s", dblBytes, suffix[i]);
    return output;
}

const char *dec_to_binary(unsigned long value, unsigned length)
{
    static char buffer[33];
    // Adjust the length.
    length = min(max(0, length), 32U);
    // Build the binary.
    for (unsigned i = 0, j = 32U - length; j < 32U; ++i, ++j) {
        buffer[i] = bit_check(value, 31 - j) ? '1' : '0';
    }
    // Close the string.
    buffer[length] = 0;
    return buffer;
}

#ifdef __KERNEL__

#include "kernel.h"

void dbg_print_regs(pt_regs *frame)
{
    pr_debug("Interrupt stack frame:\n");
    pr_debug("GS     = 0x%-04x\n", frame->gs);
    pr_debug("FS     = 0x%-04x\n", frame->fs);
    pr_debug("ES     = 0x%-04x\n", frame->es);
    pr_debug("DS     = 0x%-04x\n", frame->ds);
    pr_debug("EDI    = 0x%-09x\n", frame->edi);
    pr_debug("ESI    = 0x%-09x\n", frame->esi);
    pr_debug("EBP    = 0x%-09x\n", frame->ebp);
    pr_debug("ESP    = 0x%-09x\n", frame->esp);
    pr_debug("EBX    = 0x%-09x\n", frame->ebx);
    pr_debug("EDX    = 0x%-09x\n", frame->edx);
    pr_debug("ECX    = 0x%-09x\n", frame->ecx);
    pr_debug("EAX    = 0x%-09x\n", frame->eax);
    pr_debug("INT_NO = %-9d\n", frame->int_no);
    pr_debug("ERR_CD = %-9d\n", frame->err_code);
    pr_debug("EIP    = 0x%-09x\n", frame->eip);
    pr_debug("CS     = 0x%-04x\n", frame->cs);
    pr_debug("EFLAGS = 0x%-09x\n", frame->eflags);
    pr_debug("UESP   = 0x%-09x\n", frame->useresp);
    pr_debug("SS     = 0x%-04x\n", frame->ss);
}

#endif
