///                MentOS, The Mentoring Operating system project
/// @file   debug.c
/// @brief  Debugging primitives.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "debug.h"
#include "stdio.h"
#include "string.h"
#include "spinlock.h"

/// Serial port for QEMU.
#define SERIAL_COM1 (0x03F8)

#define DEBUG_BUFFER_SIZE 1024

static inline void dbg_putchar(char c)
{
#if (defined(DEBUG_STDIO) || defined(DEBUG_LOG))
	outportb(SERIAL_COM1, c);
#endif
}

static inline void dbg_print_header(const char *file, const char *fun, int line)
{
	static char prefix[300], final_prefix[300];

	sprintf(prefix, "[%s:%s:%d", file, fun, line);

	sprintf(final_prefix, "%-40s] ", prefix);

	for (register int it = 0; final_prefix[it] != 0; ++it) {
		dbg_putchar(final_prefix[it]);
	}
}

void _dbg_print(const char *file, const char *fun, int line, const char *msg,
				...)
{
	// Define a buffer for the formatted string.
	static char formatted[DEBUG_BUFFER_SIZE];
	static bool_t new_line = true;

	// Stage 1: FORMAT
	if (strlen(msg) >= 1024) {
		return;
	}
	// Start variabile argument's list.
	va_list ap;
	va_start(ap, msg);
	// Format the message.
	vsprintf(formatted, msg, ap);
	// End the list of arguments.
	va_end(ap);

	// Stage 2: SEND
	if (new_line) {
		dbg_print_header(file, fun, line);
		new_line = false;
	}
	for (int it = 0; (formatted[it] != 0) && (it < DEBUG_BUFFER_SIZE); ++it) {
		dbg_putchar(formatted[it]);
		if (formatted[it] != '\n') {
			continue;
		}
		if ((it + 1) >= DEBUG_BUFFER_SIZE) {
			continue;
		}
		if (formatted[it + 1] == 0) {
			new_line = true;
		} else {
			dbg_print_header(file, fun, line);
		}
	}
}

void print_intrframe(pt_regs *frame)
{
	dbg_print("Interrupt stack frame:\n");
	dbg_print("GS     = 0x%-04x\n", frame->gs);
	dbg_print("FS     = 0x%-04x\n", frame->fs);
	dbg_print("ES     = 0x%-04x\n", frame->es);
	dbg_print("DS     = 0x%-04x\n", frame->ds);
	dbg_print("EDI    = 0x%-09x\n", frame->edi);
	dbg_print("ESI    = 0x%-09x\n", frame->esi);
	dbg_print("EBP    = 0x%-09x\n", frame->ebp);
	dbg_print("ESP    = 0x%-09x\n", frame->esp);
	dbg_print("EBX    = 0x%-09x\n", frame->ebx);
	dbg_print("EDX    = 0x%-09x\n", frame->edx);
	dbg_print("ECX    = 0x%-09x\n", frame->ecx);
	dbg_print("EAX    = 0x%-09x\n", frame->eax);
	dbg_print("INT_NO = %-9d\n", frame->int_no);
	dbg_print("ERR_CD = %-9d\n", frame->err_code);
	dbg_print("EIP    = 0x%-09x\n", frame->eip);
	dbg_print("CS     = 0x%-04x\n", frame->cs);
	dbg_print("EFLAGS = 0x%-09x\n", frame->eflags);
	dbg_print("UESP   = 0x%-09x\n", frame->useresp);
	dbg_print("SS     = 0x%-04x\n", frame->ss);
}
