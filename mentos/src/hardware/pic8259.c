///                MentOS, The Mentoring Operating system project
/// @file pic8259.c
/// @brief pic8259 definitions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "pic8259.h"

/// End-of-interrupt command code.
#define EOI 0x20

/// IO base address for master PIC.
#define MASTER_PORT_COMMAND 0x20

/// I/O address for data to master.
#define MASTER_PORT_DATA (MASTER_PORT_COMMAND + 1)

/// IO base address for slave PIC.
#define SLAVE_PORT_COMMAND 0xA0

/// I/O address for data to slave.
#define SLAVE_PORT_DATA (SLAVE_PORT_COMMAND + 1)

/// ICW4 (not) needed.
#define ICW1_ICW4 0x01

/// Single (cascade) mode.
#define ICW1_SINGLE 0x02

/// Call address interval 4 (8).
#define ICW1_INTERVAL4 0x04

/// Level triggered (edge) mode.
#define ICW1_LEVEL 0x08

/// Initialization - required.
#define ICW1_INIT 0x10

/// 8086/88 (MCS-80/85) mode
#define ICW4_8086 0x01

/// Auto (normal) EOI.
#define ICW4_AUTO 0x02

/// Buffered mode/slave.
#define ICW4_BUF_SLAVE 0x08

/// Buffered mode/master.
#define ICW4_BUF_MASTER 0x0C

/// Special fully nested (not).
#define ICW4_SFNM 0x10

/// OCW3 irq ready next CMD read.
#define PIC_READ_IRR 0x0A

/// OCW3 irq service next CMD read.
#define PIC_READ_ISR 0x0B

/// The current mask of the master.
static byte_t master_cur_mask;

/// The current mask of the slave.
static byte_t slave_cur_mask;

void pic8259_init_irq()
{
	// Set the masks for the master and slave.
	master_cur_mask = 0xFF;
	slave_cur_mask = 0xFF;

	// Starts the initialization sequence (in cascade mode).
	outportb(MASTER_PORT_COMMAND, ICW1_INIT + ICW1_ICW4);
	outportb(SLAVE_PORT_COMMAND, ICW1_INIT + ICW1_ICW4);

	// ICW2: Master PIC vector offset.
	outportb(MASTER_PORT_DATA, 0x20);
	// ICW2: Slave PIC vector offset.
	outportb(SLAVE_PORT_DATA, 0x28);

	// ICW3: Tell Master PIC that there is a slave PIC at IRQ2 (0000 0100).
	outportb(MASTER_PORT_DATA, 4);
	// ICW3: Tell Slave PIC its cascade identity (0000 0010).
	outportb(SLAVE_PORT_DATA, 2);

	outportb(MASTER_PORT_DATA, ICW4_8086);
	outportb(SLAVE_PORT_DATA, ICW4_8086);

	// Restore saved masks.
	outportb(MASTER_PORT_DATA, master_cur_mask);
	outportb(SLAVE_PORT_DATA, slave_cur_mask);

	pic8259_irq_enable(IRQ_TO_SLAVE_PIC);

	outportb(0xFF, MASTER_PORT_DATA);
	outportb(0xFF, SLAVE_PORT_DATA);
}

int pic8259_irq_enable(uint32_t irq)
{
	if (irq >= IRQ_NUM) {
		return -1;
	}

	byte_t cur_mask;
	byte_t new_mask;

	if (irq < 8) {
		new_mask = ~(1 << irq);
		cur_mask = inportb(MASTER_PORT_DATA);
		outportb(MASTER_PORT_DATA, (new_mask & cur_mask));
		master_cur_mask = (new_mask & cur_mask);
	} else {
		irq -= 8;
		new_mask = ~(1 << irq);
		cur_mask = inportb(SLAVE_PORT_DATA);
		outportb(SLAVE_PORT_DATA, (new_mask & cur_mask));
		slave_cur_mask = (new_mask & cur_mask);
	}

	return 0;
}

int pic8259_irq_disable(uint32_t irq)
{
	if (irq >= IRQ_NUM) {
		return -1;
	}

	uint8_t cur_mask;
	if (irq < 8) {
		cur_mask = inportb(MASTER_PORT_DATA);
		cur_mask |= (1 << irq);
		outportb(MASTER_PORT_DATA, cur_mask & 0xFF);
	} else {
		irq = irq - 8;
		cur_mask = inportb(SLAVE_PORT_DATA);
		cur_mask |= (1 << irq);
		outportb(SLAVE_PORT_DATA, cur_mask & 0xFF);
	}
	return 0;
}

void pic8259_send_eoi(uint32_t irq)
{
	// Perhaps the most common command issued to the PIC chips is the end of
	// interrupt (EOI) command (code 0x20). This is issued to the PIC chips
	// at the end of an IRQ-based interrupt routine. If the IRQ came from the
	// Master PIC, it is sufficient to issue this command only to the Master
	// PIC; however if the IRQ came from the Slave PIC, it is necessary to
	// issue the command to both PIC chips.
	if (irq >= 8) {
		outportb(SLAVE_PORT_COMMAND, EOI);
	}
	outportb(MASTER_PORT_COMMAND, EOI);
}

/*
 * int pic8259_irq_get_current()
 * {
 *     outportb(MASTER_PORT_COMMAND, PIC_READ_ISR);
 *     uint8_t cur_irq = inportb(MASTER_PORT_COMMAND);
 *
 *     if (cur_irq == 4)
 *     {
 *         outportb(SLAVE_PORT_COMMAND, PIC_READ_ISR);
 *         cur_irq = inportb(SLAVE_PORT_COMMAND);
 *
 *         return 8 + find_first_bit(cur_irq);
 *     }
 *
 *     return find_first_bit(cur_irq);
 * }
 */
