/// @file idt.c
/// @brief Functions which manage the Interrupt Descriptor Table (IDT).
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[IDT   ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "descriptor_tables/gdt.h"
#include "descriptor_tables/idt.h"
#include "descriptor_tables/isr.h"

/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_0(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_1(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_2(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_3(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_4(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_5(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_6(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_7(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_8(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_9(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_10(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_11(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_12(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_13(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_14(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_15(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_16(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_17(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_18(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_19(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_20(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_21(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_22(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_23(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_24(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_25(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_26(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_27(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_28(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_29(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_30(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_31(pt_regs *);
/// @brief Interrupt Service Routine (ISR) for exception handling.
extern void INT_80(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_0(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_1(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_2(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_3(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_4(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_5(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_6(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_7(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_8(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_9(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_10(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_11(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_12(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_13(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_14(pt_regs *);
/// @brief Interrupt Request (IRQ) coming from the PIC.
extern void IRQ_15(pt_regs *);

/// @brief This function is in idt.asm.
/// @param idt_pointer Address of the idt.
extern void idt_flush(uint32_t idt_pointer);

/// The IDT itself.
static idt_descriptor_t idt_table[IDT_SIZE];

/// Pointer structure to give to the CPU.
idt_pointer_t idt_pointer;

/// @brief          Use this function to set an entry in the IDT.
/// @param index    Index of the IDT entry.
/// @param handler  Pointer to the entry handler.
/// @param options  Descriptors options (PRESENT, NOTPRESENT, KERNEL, USER).
/// @param seg_sel  GDT segment selector.
static inline void __idt_set_gate(uint8_t index, interrupt_handler_t handler, uint16_t options, uint8_t seg_sel)
{
    uintptr_t base_prt            = (uintptr_t)handler;
    // Assign the base values.
    idt_table[index].offset_low   = (base_prt & 0xFFFFU);
    idt_table[index].offset_high  = (base_prt >> 16U) & 0xFFFFU;
    // Set the other fields.
    idt_table[index].reserved     = 0x00;
    idt_table[index].seg_selector = seg_sel;
    idt_table[index].options      = options | IDT_PADDING;
}

void init_idt(void)
{
    // Prepare IDT vector.
    for (uint32_t it = 0; it < IDT_SIZE; ++it) {
        idt_table[it].offset_low   = 0;
        idt_table[it].seg_selector = 0;
        idt_table[it].reserved     = 0;
        idt_table[it].options      = 0;
        idt_table[it].offset_high  = 0;
    }

    // Just like the GDT, the IDT has a "limit" field that is set to the last
    // valid byte in the IDT, after adding in the start position (i.e. size-1).
    idt_pointer.limit = sizeof(idt_descriptor_t) * IDT_SIZE - 1;
    idt_pointer.base  = (uint32_t)&idt_table;

    // Initialize ISR for CPU execptions.
    isrs_init();

    // Initialize ISR for PIC interrupts.
    irq_init();

    // Register ISR [0-31] + 80, interrupts generated by CPU.
    // These interrupts will be initially managed by isr_handler.
    // The appropriate handler will be called by looking at the vector
    // isr_routines.
    __idt_set_gate(0, INT_0, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(1, INT_1, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(2, INT_2, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(3, INT_3, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(4, INT_4, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(5, INT_5, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(6, INT_6, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(7, INT_7, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(8, INT_8, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(9, INT_9, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(10, INT_10, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(11, INT_11, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(12, INT_12, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(13, INT_13, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(14, INT_14, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(15, INT_15, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(16, INT_16, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(17, INT_17, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(18, INT_18, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(19, INT_19, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(20, INT_20, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(21, INT_21, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(22, INT_22, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(23, INT_23, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(24, INT_24, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(25, INT_25, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(26, INT_26, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(27, INT_27, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(28, INT_28, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(29, INT_29, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(30, INT_30, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(31, INT_31, GDT_PRESENT | GDT_KERNEL, 0x8);

    // Registers ISR [32-47] (irq [0-15]), interrupts generated by PIC.
    // These interrupts will be initially managed by irq_handler.
    // The appropriate handler will be called by looking at the vector
    // isr_routines.
    __idt_set_gate(32, IRQ_0, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(33, IRQ_1, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(34, IRQ_2, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(35, IRQ_3, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(36, IRQ_4, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(37, IRQ_5, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(38, IRQ_6, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(39, IRQ_7, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(40, IRQ_8, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(41, IRQ_9, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(42, IRQ_10, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(43, IRQ_11, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(44, IRQ_12, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(45, IRQ_13, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(46, IRQ_14, GDT_PRESENT | GDT_KERNEL, 0x8);
    __idt_set_gate(47, IRQ_15, GDT_PRESENT | GDT_KERNEL, 0x8);

    // System call!
    __idt_set_gate(128, INT_80, GDT_PRESENT | GDT_USER, 0x8);

    // Points the processor's internal register to the new IDT.
    idt_flush((uint32_t)&idt_pointer);
}
