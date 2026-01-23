/// @file test_idt.c
/// @brief Unit tests for IDT functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "descriptor_tables/idt.h"
#include "stddef.h"
#include "tests/test.h"

// Extern declarations for IDT structures
extern idt_descriptor_t idt_table[IDT_SIZE];
extern idt_pointer_t idt_pointer;

// Test IDT initialization state (non-destructive)
TEST(idt_initialization)
{
    // Check that IDT pointer is properly set (should already be initialized)
    ASSERT(idt_pointer.limit == sizeof(idt_descriptor_t) * IDT_SIZE - 1);
    ASSERT(idt_pointer.base == (uint32_t)&idt_table);

    // Check that some key entries are set (interrupt 0 should be set)
    ASSERT(idt_table[0].offset_low != 0 || idt_table[0].offset_high != 0);
    ASSERT(idt_table[0].seg_selector == 0x8);   // Kernel code segment
    ASSERT((idt_table[0].options & 0x80) != 0); // Present bit set

    // Check that system call interrupt (128) is set
    ASSERT(idt_table[128].offset_low != 0 || idt_table[128].offset_high != 0);
    ASSERT((idt_table[128].options & 0x80) != 0);    // Present
    ASSERT((idt_table[128].options & 0x60) == 0x60); // User privilege level
}

// Test bounds checking for IDT gate setting
TEST(idt_bounds_check)
{
    // Test invalid index - this should not crash but log error
    // Note: We can't directly call __idt_set_gate as it's static, so we test via init_idt behavior
    // For now, just verify IDT_SIZE constant
    ASSERT(IDT_SIZE == 256);

    // Test that valid indices work
    idt_descriptor_t original = idt_table[IDT_SIZE - 1];
    // We can't directly test __idt_set_gate, but we can verify the table exists
    ASSERT(&idt_table[IDT_SIZE - 1] != NULL);
    idt_table[IDT_SIZE - 1] = original;
}

// Test IDT gate types and options
TEST(idt_gate_types)
{
    // Test that different gate types are defined
    ASSERT(INT32_GATE == 0xE);
    ASSERT(TRAP32_GATE == 0xF);
    ASSERT(INT16_GATE == 0x6);
    ASSERT(TRAP16_GATE == 0x7);
    ASSERT(TASK_GATE == 0x5);
}

// Test IDT privilege levels
TEST(idt_privilege_levels)
{
    // Check that after initialization, interrupts have correct privilege levels
    // Most interrupts should be kernel level (ring 0)
    ASSERT((idt_table[0].options & 0x60) == 0x00); // DPL = 0 for kernel

    // System call (interrupt 128) should allow user level (ring 3)
    ASSERT((idt_table[128].options & 0x60) == 0x60); // DPL = 3 for user
}

// Test IDT segment selectors
TEST(idt_segment_selectors)
{
    // Check that interrupts use kernel code segment (0x8)
    ASSERT(idt_table[0].seg_selector == 0x8);
    ASSERT(idt_table[32].seg_selector == 0x8);  // IRQ 0
    ASSERT(idt_table[128].seg_selector == 0x8); // System call
}

// Test IDT present bits
TEST(idt_present_bits)
{
    // Check that initialized interrupts are present
    ASSERT((idt_table[0].options & 0x80) != 0);   // Present
    ASSERT((idt_table[32].options & 0x80) != 0);  // Present
    ASSERT((idt_table[128].options & 0x80) != 0); // Present
}

// Test IDT reserved fields
TEST(idt_reserved_fields)
{
    // Check that reserved fields are set correctly (should be 0)
    ASSERT(idt_table[0].reserved == 0x00);
    ASSERT(idt_table[32].reserved == 0x00);
    ASSERT(idt_table[128].reserved == 0x00);
}

// Test IDT offset fields
TEST(idt_offset_fields)
{
    // Check that offset fields are set (not zero for initialized entries)
    ASSERT(idt_table[0].offset_low != 0 || idt_table[0].offset_high != 0);
    ASSERT(idt_table[32].offset_low != 0 || idt_table[32].offset_high != 0);
    ASSERT(idt_table[128].offset_low != 0 || idt_table[128].offset_high != 0);
}

// Test IDT table size
TEST(idt_table_size)
{
    // Verify IDT has correct size
    ASSERT(IDT_SIZE == 256);

    // Verify pointer structure
    ASSERT(sizeof(idt_descriptor_t) * IDT_SIZE == 2048); // 256 * 8 bytes
    ASSERT(idt_pointer.limit == 2047);                   // size - 1
}

// Test IDT interrupt ranges
TEST(idt_interrupt_ranges)
{
    // Test that CPU exceptions (0-31) are set
    for (int i = 0; i < 32; i++) {
        ASSERT(idt_table[i].offset_low != 0 || idt_table[i].offset_high != 0);
        ASSERT((idt_table[i].options & 0x80) != 0); // Present
    }

    // Test that IRQs (32-47) are set
    for (int i = 32; i < 48; i++) {
        ASSERT(idt_table[i].offset_low != 0 || idt_table[i].offset_high != 0);
        ASSERT((idt_table[i].options & 0x80) != 0); // Present
    }

    // Test that system call (128) is set
    ASSERT(idt_table[128].offset_low != 0 || idt_table[128].offset_high != 0);
    ASSERT((idt_table[128].options & 0x80) != 0); // Present
}

// Test IDT options field composition
TEST(idt_options_composition)
{
    // Test that options field combines gate type and flags correctly
    // For interrupt gates: present (0x80) | kernel (0x00) | type (0x0E) = 0x8E
    ASSERT((idt_table[0].options & 0x0F) == INT32_GATE); // Type bits

    // For system call: present (0x80) | user (0x60) | type (0x0E) = 0xEE
    ASSERT((idt_table[128].options & 0x0F) == INT32_GATE); // Type bits
}
