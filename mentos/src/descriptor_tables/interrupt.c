///                MentOS, The Mentoring Operating system project
/// @file isr.c
/// @brief Functions which manage the Interrupt Service Routines (ISRs).
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "isr.h"
#include "idt.h"
#include "printk.h"
#include "pic8259.h"
#include "pic8259.h"
#include "scheduler.h"

// TODO: lists, double-linked lists should be a Kernel data struture!
/// @brief shared interrupt handlers are stored into a linked list.
///        irq_struct_t is a node of the list
typedef struct irq_struct_t {
	/// Puntatore alla funzione handler di un IRQ.
	interrupt_handler_t handler;

	/// Puntatore alla descrizione dell'handler.
	char *description;

	/// Prossimo handler per questo IRQ
	struct irq_struct_t *next;
} irq_struct_t;

// For each IRQ, a chain of handlers.
static irq_struct_t *shared_interrupt_handlers[IRQ_NUM];

// Default handler for interrupts and exceptions.
void default_irq_handler(pt_regs *f)
{
	uint32_t irq_line = f->int_no - 32;

	printk("Kernel PANIC!\n no handler for IRQ [%d]\n", irq_line);

	// Stop kernel execution.
	while (1)
		;
	// TODO: call the kernel panic method!
}

void irqs_init()
{
	// Setting the default_irq_handler as default handler for each IRQ line.
	irq_struct_t *irq_struct = NULL;
	for (uint32_t i = 0; i < IRQ_NUM; ++i) {
		irq_struct = (irq_struct_t *)kmalloc(sizeof(irq_struct_t));
		irq_struct->handler = default_irq_handler;
		irq_struct->next = NULL;
		shared_interrupt_handlers[i] = irq_struct;
	}
}

int irq_install_handler(uint32_t i, interrupt_handler_t handler,
			char *description)
{
	// We have maximun IRQ_NUM IRQ lines.
	if (i > IRQ_NUM) {
		return -1;
	}

	// The current handler for this IRQ line.
	irq_struct_t *current_irq_struct = shared_interrupt_handlers[i];

	// Is the current handler the default one?
	if (current_irq_struct->handler == default_irq_handler) {
		// Set the given handler as a new handler for i-th IRQ.
		current_irq_struct->handler = handler;
		// Return. Nothing else to do.
		return 0;
	}

	// Move current_irq_struct to get the last handler for this IRQ line.
	while (current_irq_struct->next != NULL) {
		current_irq_struct = current_irq_struct->next;
	}

	// Create a new irq_struct_t to save the given handler.
	irq_struct_t *irq_struct =
		(irq_struct_t *)kmalloc(sizeof(irq_struct_t));
	irq_struct->next = NULL;
	irq_struct->description = description;
	irq_struct->handler = handler;

	// Store the given handler.
	current_irq_struct->next = irq_struct;
	return 0;
}

void irq_handler(pt_regs *f)
{
	// Keep in mind,
	// because of irq mapping, the first PIC's irq line is shifted by 32.
	uint32_t irq_line = f->int_no - 32;

	// Actually, we may have several handlers for a same irq line.
	// The Kernel should provide the dev_id to each handler in order to
	// let it know if its own device generated the interrupt.
	// TODO: get dev_id

	irq_struct_t *irq_struct = shared_interrupt_handlers[irq_line];
	do {
		// Call the interrupt function.
		irq_struct->handler(f);
		// Move to the next interrupt function.
		irq_struct = irq_struct->next;
	} while (irq_struct != NULL);

	// Send the end-of-interrupt to PIC.
	pic8259_send_eoi(irq_line);
}
