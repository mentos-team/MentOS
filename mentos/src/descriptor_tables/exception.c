///                MentOS, The Mentoring Operating system project
/// @file isr.c
/// @brief Functions which manage the Interrupt Service Routines (ISRs).
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "isr.h"
#include "idt.h"
#include "stdio.h"
#include "debug.h"

// Default error messages for exceptions.
static const char *exception_messages[32] = { "Division by zero",
					      "Debug",
					      "Non-maskable interrupt",
					      "Breakpoint",
					      "Detected overflow",
					      "Out-of-bounds",
					      "Invalid opcode",
					      "No coprocessor",
					      "Double fault",
					      "Coprocessor segment overrun",
					      "Bad TSS",
					      "Segment not present",
					      "Stack fault",
					      "General protection fault",
					      "Page fault",
					      "Unknown interrupt",
					      "Coprocessor fault",
					      "Alignment check",
					      "Machine check",
					      "Reserved",
					      "Reserved",
					      "Reserved",
					      "Reserved",
					      "Reserved",
					      "Reserved",
					      "Reserved",
					      "Reserved",
					      "Reserved",
					      "Reserved",
					      "Reserved",
					      "Security exception",
					      "Triple fault" };

// Array of interrupt service routines for execptions and interrupts.
static interrupt_handler_t isr_routines[IDT_SIZE];
static char *isr_routines_description[IDT_SIZE];

// Default handler for exceptions.
void default_isr_handler(pt_regs *f)
{
	uint32_t irq_line = f->int_no;

	printf("Kernel PANIC!\n no handler for execption [%d]\n", irq_line);
	printf("Description: %s\n", (irq_line < 32) ?
					    exception_messages[irq_line] :
					    "no description");
	// Stop kernel execution.
	while (1)
		;

	// TODO: call the kernel panic method!
}

void isrs_init()
{
	// Setting the default_isr_handler as default handler.
	for (uint32_t i = 0; i < IDT_SIZE; ++i) {
		isr_routines[i] = default_isr_handler;
	}
}

int isr_install_handler(uint32_t i, interrupt_handler_t handler,
			char *description)
{
	// Sanity check.
	if (!(i >= 0 && i <= 31) && i != 80) {
		return -1;
	}
	isr_routines[i] = handler;
	isr_routines_description[i] = description;
	return 0;
}

void isr_handler(pt_regs *f)
{
	uint32_t isr_number = f->int_no;
	if (isr_number != 80) {
		dbg_print("calling ISR %d\n", isr_number);
	}
	isr_routines[isr_number](f);
}
