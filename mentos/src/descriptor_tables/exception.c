/// @file exception.c
/// @brief Functions which manage the Interrupt Service Routines (ISRs).
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[EXEPT ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "system/panic.h"
#include "descriptor_tables/isr.h"
#include "descriptor_tables/idt.h"
#include "stdio.h"
#include "io/debug.h"

// Default error messages for exceptions.
static const char *exception_messages[32] = {
    "Division by zero",
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
    "Triple fault"
};

/// @brief Array of interrupt service routines for execptions and interrupts.
static interrupt_handler_t isr_routines[IDT_SIZE];
/// @brief Descriptions of routines.
static char *isr_routines_description[IDT_SIZE];

/// @brief Default handler for exceptions.
/// @param f CPU registers when calling this function.
static inline void default_isr_handler(pt_regs *f)
{
    uint32_t irq_line = f->int_no;

    dbg_print_regs(f);

    printf("Kernel PANIC!\n no handler for execption [%d]\n", irq_line);
    printf("Description: %s\n", (irq_line < 32) ? exception_messages[irq_line] : "no description");
    // Stop kernel execution.

    kernel_panic("Kernel PANIC!\n no handler for execption");

    while (1) {}
}

/// @brief Interrupt Service Routines handler called from the assembly.
/// @param f CPU registers when calling this function.
void isr_handler(pt_regs *f)
{
    uint32_t isr_number = f->int_no;
    if (isr_number != 80) {
        //		pr_default("calling ISR %d\n", isr_number);
    }
    //    pr_default("calling ISR %d\n", isr_number);
    isr_routines[isr_number](f);
    //    pr_default("end calling ISR %d\n", isr_number);
}

void isrs_init()
{
    // Setting the default_isr_handler as default handler.
    for (uint32_t i = 0; i < IDT_SIZE; ++i) {
        isr_routines[i] = default_isr_handler;
    }
}

int isr_install_handler(unsigned i, interrupt_handler_t handler, char *description)
{
    // Sanity check.
    if (i > 31 && i != 80) {
        return -1;
    }
    isr_routines[i]             = handler;
    isr_routines_description[i] = description;
    return 0;
}

int isr_uninstall_handler(unsigned i)
{
    if (i > 31 && i != 80) {
        return -1;
    }
    isr_routines[i]             = default_isr_handler;
    isr_routines_description[i] = "NONE";
    return 0;
}
