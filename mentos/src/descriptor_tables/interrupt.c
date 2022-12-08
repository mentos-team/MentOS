/// @file interrupt.c
/// @brief Functions which manage the Interrupt Service Routines (ISRs).
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[IRQ   ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "descriptor_tables/isr.h"

#include "process/scheduler.h"
#include "hardware/pic8259.h"
#include "system/printk.h"
#include "assert.h"
#include "stdio.h"
#include "io/debug.h"
#include "descriptor_tables/idt.h"

/// @brief Shared interrupt handlers, stored into a double-linked list.
typedef struct irq_struct_t {
    /// Pointer to the IRQ handler.
    interrupt_handler_t handler;
    /// Pointer to the description of the handler.
    char *description;
    /// List handler.
    list_head siblings;
} irq_struct_t;

/// For each IRQ, a chain of handlers.
static list_head shared_interrupt_handlers[IRQ_NUM];
/// Cache where we will store the data regarding an irq service.
static kmem_cache_t *irq_cache;

/// @brief Creates a new irq struct.
static inline irq_struct_t *__irq_struct_alloc()
{
    // Allocate the structure.
    irq_struct_t *irq_struct = kmem_cache_alloc(irq_cache, GFP_KERNEL);
    assert(irq_struct && "Failed to allocate memory for IRQ structure.");
    // Initialize its fields.
    irq_struct->description = NULL;
    irq_struct->handler     = NULL;
    list_head_init(&irq_struct->siblings);
    return irq_struct;
}

/// @brief Destroys an irq struct.
static inline void __irq_struct_dealloc(irq_struct_t *irq_struct)
{
    list_head_remove(&irq_struct->siblings);
    kmem_cache_free(irq_struct);
}

void irq_init()
{
    // Initialize the cache.
    irq_cache = KMEM_CREATE(irq_struct_t);
    // Initializing the list for each irq number.
    for (uint32_t i = 0; i < IRQ_NUM; ++i) {
        list_head_init(&shared_interrupt_handlers[i]);
    }
}

int irq_install_handler(unsigned i, interrupt_handler_t handler, char *description)
{
    // We have maximun IRQ_NUM IRQ lines.
    if (i >= IRQ_NUM) {
        pr_err("There are no handler for IRQ `%d`\n", i);
        return -1;
    }
    // Create a new irq_struct_t to save the given handler.
    irq_struct_t *irq_struct = __irq_struct_alloc();
    if (irq_struct == NULL) {
        pr_err("Failed to allocate more IRQ structures for IRQ `%d`\n", i);
        return -1;
    }
    irq_struct->description = description;
    irq_struct->handler     = handler;
    // Add the handler to the list of his siblings.
    list_head_insert_before(&irq_struct->siblings, &shared_interrupt_handlers[i]);
    return 0;
}

int irq_uninstall_handler(unsigned i, interrupt_handler_t handler)
{
    // We have maximun IRQ_NUM IRQ lines.
    if (i >= IRQ_NUM) {
        pr_err("There are no handler for IRQ `%d`\n", i);
        return -1;
    }
    if (list_head_empty(&shared_interrupt_handlers[i])) {
        pr_err("There are no handler for IRQ `%d`\n", i);
        return -1;
    }
    list_for_each_decl(it, &shared_interrupt_handlers[i])
    {
        // Get the interrupt structure.
        irq_struct_t *irq_struct = list_entry(it, irq_struct_t, siblings);
        assert(irq_struct && "Something went wrong.");
        if (irq_struct->handler == handler) {
            list_head_remove(&irq_struct->siblings);
        }
        __irq_struct_dealloc(irq_struct);
    }
    return 0;
}

void irq_handler(pt_regs *f)
{
    // Keep in mind,
    // because of irq mapping, the first PIC's irq line is shifted by 32.
    unsigned irq_line = f->int_no - 32;
    assert((irq_line < IRQ_NUM) && "Unidentified IRQ number.");
    // Actually, we may have several handlers for a same irq line.
    // The Kernel should provide the dev_id to each handler in order to
    // let it know if its own device generated the interrupt.
    // TODO: get dev_id
    if (list_head_empty(&shared_interrupt_handlers[irq_line])) {
        pr_err("Thre are no handler for IRQ `%d`\n", irq_line);
    } else {
        list_for_each_decl(it, &shared_interrupt_handlers[irq_line])
        {
            // Get the interrupt structure.
            irq_struct_t *irq_struct = list_entry(it, irq_struct_t, siblings);
            assert(irq_struct && "Something went wrong.");
            // Call the interrupt function.
            irq_struct->handler(f);
        }
    }
    // Send the end-of-interrupt to PIC.
    pic8259_send_eoi(irq_line);
}
