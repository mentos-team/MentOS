/// @file   debug.c
/// @brief  Debugging primitives.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/bitops.h"
#include "io/debug.h"
#include "klib/spinlock.h"
#include "io/port_io.h"
#include "kernel.h"
#include "string.h"
#include "stdio.h"
#include "io/video.h"
#include "math.h"

/// Serial port for QEMU.
#define SERIAL_COM1 (0x03F8)
/// Determines the log level.
static int max_log_level = LOGLEVEL_DEBUG;

void dbg_putchar(char c)
{
    outportb(SERIAL_COM1, (uint8_t)c);
}

void dbg_puts(const char *s)
{
    while ((*s) != 0)
        dbg_putchar(*s++);
}

static inline void __debug_print_header(const char *file, const char *fun, int line, int log_level, char *header)
{
    static char tmp_prefix[BUFSIZ], final_prefix[BUFSIZ];
    if (log_level == LOGLEVEL_EMERG)
        dbg_puts(FG_RED_BRIGHT);
    else if (log_level == LOGLEVEL_ALERT)
        dbg_puts(FG_RED_BRIGHT);
    else if (log_level == LOGLEVEL_CRIT)
        dbg_puts(FG_RED);
    else if (log_level == LOGLEVEL_ERR)
        dbg_puts(FG_RED);
    else if (log_level == LOGLEVEL_WARNING)
        dbg_puts(FG_YELLOW_BRIGHT);
    else if (log_level == LOGLEVEL_DEBUG)
        dbg_puts(FG_CYAN);
    else if (log_level == LOGLEVEL_NOTICE)
        dbg_puts(FG_RESET);
    else if (log_level == LOGLEVEL_INFO)
        dbg_puts(FG_RESET);
    else
        dbg_puts(FG_RESET);
    dbg_putchar('[');
    dbg_puts(
        (log_level == LOGLEVEL_EMERG)   ? " EM " /*"EMERG  "*/ :
        (log_level == LOGLEVEL_ALERT)   ? " AL " /*"ALERT  "*/ :
        (log_level == LOGLEVEL_CRIT)    ? " CR " /*"CRIT   "*/ :
        (log_level == LOGLEVEL_ERR)     ? " ER " /*"ERR    "*/ :
        (log_level == LOGLEVEL_WARNING) ? " WR " /*"WARNING"*/ :
        (log_level == LOGLEVEL_NOTICE)  ? " NT " /*"NOTICE "*/ :
        (log_level == LOGLEVEL_INFO)    ? " IN " /*"INFO   "*/ :
        (log_level == LOGLEVEL_DEBUG)   ? " DB " /*"DEBUG  "*/ :
                                          " DF " /*"DEFAULT*/);
    dbg_putchar('|');
    sprintf(tmp_prefix, "%s:%d", file, line);
    sprintf(final_prefix, " %-20s ", tmp_prefix);
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
    if ((level >= LOGLEVEL_EMERG) && (level <= LOGLEVEL_DEBUG))
        max_log_level = level;
}

int get_log_level()
{
    return max_log_level;
}

void dbg_printf(const char *file, const char *fun, int line, char *header, const char *format, ...)
{
    // Define a buffer for the formatted string.
    static char formatted[BUFSIZ];
    static short new_line = 1;

    // Stage 1: FORMAT
    if (strlen(format) >= BUFSIZ)
        return;

    // Check the log level.
    int log_level = LOGLEVEL_DEFAULT;
    if ((format[0] != '\0') && (format[0] == KERN_SOH_ASCII)) {
        // Remove the Start Of Header.
        ++format;
        // Compute the log level.
        log_level = (format[0] - '0');
        // Check the log_level.
        if ((log_level < LOGLEVEL_EMERG) || (log_level > LOGLEVEL_DEBUG))
            log_level = 8;
        // Remove the log_level;
        ++format;
    }
    if (log_level > max_log_level) {
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
        new_line = false;
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
            new_line = true;
        } else {
            __debug_print_header(file, fun, line, log_level, header);
        }
    }
}

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

const char *to_human_size(unsigned long bytes)
{
    static char output[200];
    const char *suffix[] = { "B", "KB", "MB", "GB", "TB" };
    char length          = sizeof(suffix) / sizeof(suffix[0]);
    int i                = 0;
    double dblBytes      = bytes;
    if (bytes > 1024) {
        for (i = 0; (bytes / 1024) > 0 && i < length - 1; i++, bytes /= 1024)
            dblBytes = bytes / 1024.0;
    }
    sprintf(output, "%.02lf %2s", dblBytes, suffix[i]);
    return output;
}

const char *dec_to_binary(unsigned long value, unsigned length)
{
    static char buffer[33];
    memset(buffer, 0, 33);
    for (int i = 0, j = 32 - min(max(0, length), 32); j < 32; ++i, ++j)
        buffer[i] = bit_check(value, 31 - j) ? '1' : '0';
    return buffer;
}
