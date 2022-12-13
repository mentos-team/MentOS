/// @file idt.h
/// @brief Data structures concerning the Interrupt Descriptor Table (IDT).
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup descriptor_tables Descriptor Tables
/// @{
/// @addtogroup idt Interrupt Descriptor Table (IDT)
/// @brief Is a data structure used by the x86 architecture to implement an
/// interrupt vector table. The IDT is used by the processor to determine the
/// correct response to interrupts and exceptions.
/// @{

#pragma once

#include "stdint.h"

/// The maximum dimension of the IDT.
#define IDT_SIZE 256
/// When an exception occurs whose entry is a Task Gate, a task switch results.
#define TASK_GATE 0x5
/// Used to specify an interrupt service routine (16-bit).
#define INT16_GATE 0x6
/// @brief Similar to an Interrupt gate (16-bit).
#define TRAP16_GATE 0x7
/// Used to specify an interrupt service routine (32-bit).
#define INT32_GATE 0xE
/// @brief Similar to an Interrupt gate (32-bit).
#define TRAP32_GATE 0xF

/*
 * Trap and Interrupt gates are similar, and their descriptors are
 * structurally the same, they differ only in the "type" field. The
 * difference is that for interrupt gates, interrupts are automatically
 * disabled upon entry and reenabled upon IRET which restores the saved EFLAGS.
 */

/// @brief This structure describes one interrupt gate.
typedef struct idt_descriptor_t {
    /// The lower 16 bits of the ISR's address.
    uint16_t offset_low;
    /// The GDT segment selector that the CPU will load into CS before calling the ISR.
    uint16_t seg_selector;
    /// This will ALWAYS be set to 0.
    uint8_t reserved;
    /// Descriptor options: |P|DPL|01110| (P: present, DPL: required Ring).
    uint8_t options;
    /// The higher 16 bits of the ISR's address.
    uint16_t offset_high;
} __attribute__((packed)) idt_descriptor_t;

/// @brief A pointer structure used for informing the CPU about our IDT.
typedef struct idt_pointer_t {
    /// The size of the IDT (entry number).
    uint16_t limit;
    /// The start address of the IDT.
    uint32_t base;
} __attribute__((packed)) idt_pointer_t;

/// @brief Initialise the interrupt descriptor table.
void init_idt();

/// @}
/// @}
