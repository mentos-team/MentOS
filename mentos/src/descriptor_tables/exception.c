/// @file exception.c
/// @brief Functions which manage the Interrupt Service Routines (ISRs).
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[EXEPT ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "descriptor_tables/idt.h"
#include "descriptor_tables/isr.h"
#include "process/scheduler.h"
#include "stdio.h"
#include "system/panic.h"

/// @brief Default error messages for exceptions.
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
    "Triple fault"};

/// @brief Array of interrupt service routines for execptions and interrupts.
static interrupt_handler_t isr_routines[IDT_SIZE];
/// @brief Descriptions of routines.
static char *isr_routines_description[IDT_SIZE];

/// @brief Default handler for exceptions.
/// @param f CPU registers when calling this function.
static inline void default_isr_handler(pt_regs_t *f)
{
    uint32_t irq_line = f->int_no;
    PRINT_REGS(pr_emerg, f);
    pr_emerg(
        "No handler for execption: %d (%s)\n", irq_line,
        (irq_line < 32) ? exception_messages[irq_line] : "no description");
    kernel_panic("Missing ISR handler.");
}

/// @brief Handles a General Protection Fault (exception 13).
/// @param frame The CPU registers at the time of the exception.
void handle_gp_fault(pt_regs_t *frame)
{
    // Log the general protection fault details
    pr_info("General Protection Fault (Exception 13) occurred!\n");
    pr_info("Faulting address: 0x%-09x\n", frame->eip);
    pr_info("Error code: 0x%-04x\n", frame->err_code);

    // Check if the privilege level is 3 (user mode)
    if ((frame->cs & 0x3) == 0x3) {
        pr_info("Process terminated due to General Protection Fault.\n");
        // Get the current process.
        task_struct *task = scheduler_get_current_process();
        assert(task && "There is no current task.");
        // Attempt to recover by terminating the user-mode process.
        sys_kill(task->pid, SIGSEGV);
        // Now, we know the process needs to be removed from the list of
        // running processes. We pushed the SEGV signal in the queues of
        // signal to send to the process. To properly handle the signal,
        // just run scheduler.
        scheduler_run(frame);
    } else {
        // Print all register values for debugging.
        PRINT_REGS(pr_crit, frame);
        pr_crit("Kernel mode fault. System halt.\n");
        // Here, we halt the CPU to prevent further damage.
        kernel_panic("General protection fault.");
    }
}

/// @brief Interrupt Service Routines handler called from the assembly.
/// @param f CPU registers when calling this function.
void isr_handler(pt_regs_t *f)
{
    uint32_t isr_number = f->int_no;
    if (isr_number != 80) {
        //		pr_default("calling ISR %d\n", isr_number);
    }
    //    pr_default("calling ISR %d\n", isr_number);
    isr_routines[isr_number](f);
    //    pr_default("end calling ISR %d\n", isr_number);
}

void isrs_init(void)
{
    // Setting the default_isr_handler as default handler.
    for (uint32_t i = 0; i < IDT_SIZE; ++i) {
        isr_routines[i] = default_isr_handler;
    }
    isr_routines[13] = handle_gp_fault;
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
