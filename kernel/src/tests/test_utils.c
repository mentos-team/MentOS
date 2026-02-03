/// @file test_utils.c
/// @brief Implementation of test utility functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUTIL ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "descriptor_tables/gdt.h"
#include "descriptor_tables/idt.h"
#include "string.h"
#include "tests/test_utils.h"

// External declarations for actual kernel structures
extern gdt_descriptor_t gdt[GDT_SIZE];
extern idt_descriptor_t idt_table[IDT_SIZE];

int test_gdt_safe_copy(size_t src_idx, void *dest_buffer)
{
    if (src_idx >= GDT_SIZE) {
        pr_warning("Invalid GDT index %zu (max: %d)\n", src_idx, GDT_SIZE - 1);
        return -1;
    }

    if (dest_buffer == NULL) {
        pr_warning("NULL destination buffer for GDT copy\n");
        return -1;
    }

    // Safe memcpy of the GDT entry
    memcpy(dest_buffer, &gdt[src_idx], sizeof(gdt_descriptor_t));
    return 0;
}

int test_idt_safe_copy(size_t src_idx, void *dest_buffer)
{
    if (src_idx >= IDT_SIZE) {
        pr_warning("Invalid IDT index %zu (max: %d)\n", src_idx, IDT_SIZE - 1);
        return -1;
    }

    if (dest_buffer == NULL) {
        pr_warning("NULL destination buffer for IDT copy\n");
        return -1;
    }

    // Safe memcpy of the IDT entry
    memcpy(dest_buffer, &idt_table[src_idx], sizeof(idt_descriptor_t));
    return 0;
}
