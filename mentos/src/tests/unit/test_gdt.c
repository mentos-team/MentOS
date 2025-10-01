/// @file test_gdt.c
/// @brief Unit tests for GDT functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "descriptor_tables/gdt.h"
#include "tests/test.h"

// Extern declaration for gdt array
extern gdt_descriptor_t gdt[GDT_SIZE];

// Test gdt_set_gate function
TEST(gdt_set_gate)
{
    // Save original GDT entry for restoration
    gdt_descriptor_t original = gdt[1];

    // Test setting a code segment
    gdt_set_gate(1, 0x1000, 0x2000, 0x9A, 0xCF);
    ASSERT(gdt[1].base_low == 0x1000);
    ASSERT(gdt[1].base_middle == 0x00);
    ASSERT(gdt[1].base_high == 0x00);
    ASSERT(gdt[1].limit_low == 0x2000);
    ASSERT(gdt[1].access == 0x9A);
    ASSERT(gdt[1].granularity == 0xC0); // 0xCF & 0xF0 = 0xC0, since limit high bits are 0

    // Restore original
    gdt[1] = original;
}

// Test bounds checking for gdt_set_gate
TEST(gdt_bounds_check)
{
    // Test invalid index - this should not crash but log error
    gdt_set_gate(GDT_SIZE, 0x1000, 0x2000, 0x9A, 0xCF);
    gdt_set_gate(255, 0x1000, 0x2000, 0x9A, 0xCF);

    // Test edge case - last valid index
    gdt_descriptor_t original = gdt[GDT_SIZE - 1];
    gdt_set_gate(GDT_SIZE - 1, 0x1000, 0x2000, 0x9A, 0xCF);
    ASSERT(gdt[GDT_SIZE - 1].base_low == 0x1000);
    gdt[GDT_SIZE - 1] = original;
}

// Test different segment types
TEST(gdt_segment_types)
{
    gdt_descriptor_t original = gdt[2];

    // Test data segment
    gdt_set_gate(2, 0x2000, 0x3000, GDT_PRESENT | GDT_KERNEL | GDT_DATA, GDT_GRANULARITY | GDT_OPERAND_SIZE);
    ASSERT(gdt[2].base_low == 0x2000);
    ASSERT(gdt[2].limit_low == 0x3000);
    ASSERT(gdt[2].access == (GDT_PRESENT | GDT_KERNEL | GDT_DATA));

    // Test user mode code segment
    gdt_set_gate(2, 0x4000, 0x5000, GDT_PRESENT | GDT_USER | GDT_CODE | GDT_RW, GDT_GRANULARITY | GDT_OPERAND_SIZE);
    ASSERT(gdt[2].access == (GDT_PRESENT | GDT_USER | GDT_CODE | GDT_RW));

    gdt[2] = original;
}

// Test base address splitting across fields
TEST(gdt_base_address_fields)
{
    gdt_descriptor_t original = gdt[3];

    // Test with a 32-bit base address
    uint32_t base = 0x12345678;
    gdt_set_gate(3, base, 0x1000, 0x9A, 0xCF);

    ASSERT(gdt[3].base_low == (base & 0xFFFF));          // Low 16 bits
    ASSERT(gdt[3].base_middle == ((base >> 16) & 0xFF)); // Middle 8 bits
    ASSERT(gdt[3].base_high == ((base >> 24) & 0xFF));   // High 8 bits

    gdt[3] = original;
}

// Test limit field handling
TEST(gdt_limit_fields)
{
    gdt_descriptor_t original = gdt[4];

    // Test with different limit values
    uint32_t limit = 0x12345;
    gdt_set_gate(4, 0x1000, limit, 0x9A, 0xCF);

    ASSERT(gdt[4].limit_low == (limit & 0xFFFF));                  // Low 16 bits
    ASSERT((gdt[4].granularity & 0x0F) == ((limit >> 16) & 0x0F)); // High 4 bits in granularity

    gdt[4] = original;
}

// Test granularity field composition
TEST(gdt_granularity_composition)
{
    gdt_descriptor_t original = gdt[5];

    uint32_t limit = 0xABCDE;
    uint8_t granul = 0xF0;
    gdt_set_gate(5, 0x1000, limit, 0x9A, granul);

    // Granularity should be: (granul & 0xF0) | ((limit >> 16) & 0x0F)
    uint8_t expected_granularity = (granul & 0xF0) | ((limit >> 16) & 0x0F);
    ASSERT(gdt[5].granularity == expected_granularity);

    gdt[5] = original;
}

// Test NULL descriptor preservation
TEST(gdt_null_descriptor)
{
    // Ensure the NULL descriptor (index 0) remains zero
    gdt_descriptor_t null_before = gdt[0];

    // Try to modify NULL descriptor (should work but violates convention)
    gdt_set_gate(0, 0x1000, 0x2000, 0x9A, 0xCF);

    // In a real system, we might want to prevent this, but for now we just test it works
    ASSERT(gdt[0].base_low == 0x1000);

    // Restore NULL descriptor to maintain system integrity
    gdt[0] = null_before;
    ASSERT(gdt[0].base_low == 0);
    ASSERT(gdt[0].base_middle == 0);
    ASSERT(gdt[0].base_high == 0);
    ASSERT(gdt[0].limit_low == 0);
    ASSERT(gdt[0].access == 0);
    ASSERT(gdt[0].granularity == 0);
}

// Test GDT initialization state
TEST(gdt_initialization_state)
{
    // Test that standard entries are properly initialized
    // Note: We're testing the current state, not re-initializing

    // Check NULL descriptor (index 0)
    ASSERT(gdt[0].base_low == 0);
    ASSERT(gdt[0].base_middle == 0);
    ASSERT(gdt[0].base_high == 0);
    ASSERT(gdt[0].limit_low == 0);
    ASSERT(gdt[0].access == 0);
    ASSERT(gdt[0].granularity == 0);

    // Check kernel code segment (index 1)
    ASSERT(gdt[1].base_low == 0);
    ASSERT(gdt[1].base_middle == 0);
    ASSERT(gdt[1].base_high == 0);
    ASSERT(gdt[1].access & GDT_PRESENT); // Present bit should be set
    ASSERT(!(gdt[1].access & GDT_USER)); // Should be kernel mode (user bits clear)
    ASSERT(gdt[1].access & GDT_S);       // Should be segment descriptor
    ASSERT(gdt[1].access & GDT_EX);      // Should be executable (code segment)
    ASSERT(gdt[1].access & GDT_RW);      // Should be readable (code segment)
    ASSERT((gdt[1].granularity & 0xF0) == (GDT_GRANULARITY | GDT_OPERAND_SIZE));

    // Check kernel data segment (index 2)
    ASSERT(gdt[2].base_low == 0);
    // Check individual bits rather than exact value since accessed bit might be set
    ASSERT(gdt[2].access & GDT_PRESENT); // Present bit should be set
    ASSERT(!(gdt[2].access & GDT_USER)); // Should be kernel mode (user bits clear)
    ASSERT(gdt[2].access & GDT_S);       // Should be segment descriptor
    ASSERT(!(gdt[2].access & GDT_EX));   // Should not be executable (data segment)
    ASSERT(gdt[2].access & GDT_RW);      // Should be writable (data segment)

    // Check user code segment (index 3)
    ASSERT(gdt[3].access & GDT_PRESENT); // Present bit should be set
    ASSERT(gdt[3].access & GDT_USER);    // Should be user mode
    ASSERT(gdt[3].access & GDT_S);       // Should be segment descriptor
    ASSERT(gdt[3].access & GDT_EX);      // Should be executable (code segment)
    ASSERT(gdt[3].access & GDT_RW);      // Should be readable (code segment)

    // Check user data segment (index 4)
    ASSERT(gdt[4].access & GDT_PRESENT); // Present bit should be set
    ASSERT(gdt[4].access & GDT_USER);    // Should be user mode
    ASSERT(gdt[4].access & GDT_S);       // Should be segment descriptor
    ASSERT(!(gdt[4].access & GDT_EX));   // Should not be executable (data segment)
    ASSERT(gdt[4].access & GDT_RW);      // Should be writable (data segment)
}

// Test privilege level encoding
TEST(gdt_privilege_levels)
{
    gdt_descriptor_t original = gdt[6];

    // Test kernel privilege (Ring 0)
    gdt_set_gate(6, 0x1000, 0x2000, GDT_PRESENT | GDT_KERNEL | GDT_CODE, 0);
    ASSERT((gdt[6].access & 0x60) == GDT_KERNEL); // Bits 5-6 should be 00

    // Test user privilege (Ring 3)
    gdt_set_gate(6, 0x1000, 0x2000, GDT_PRESENT | GDT_USER | GDT_CODE, 0);
    ASSERT((gdt[6].access & 0x60) == GDT_USER); // Bits 5-6 should be 11

    gdt[6] = original;
}

// Test segment type flags
TEST(gdt_segment_flags)
{
    gdt_descriptor_t original = gdt[7];

    // Test executable code segment
    gdt_set_gate(7, 0, 0x1000, GDT_PRESENT | GDT_KERNEL | GDT_CODE, 0);
    ASSERT(gdt[7].access & GDT_EX); // Executable bit should be set
    ASSERT(gdt[7].access & GDT_S);  // Segment descriptor bit should be set

    // Test data segment (non-executable)
    gdt_set_gate(7, 0, 0x1000, GDT_PRESENT | GDT_KERNEL | GDT_DATA, 0);
    ASSERT(!(gdt[7].access & GDT_EX)); // Executable bit should be clear
    ASSERT(gdt[7].access & GDT_S);     // Segment descriptor bit should be set

    gdt[7] = original;
}

// Test limit boundary values
TEST(gdt_limit_boundaries)
{
    gdt_descriptor_t original = gdt[8];

    // Test minimum limit (0)
    gdt_set_gate(8, 0x1000, 0, 0x9A, 0);
    ASSERT(gdt[8].limit_low == 0);
    ASSERT((gdt[8].granularity & 0x0F) == 0);

    // Test maximum 20-bit limit
    uint32_t max_limit = 0xFFFFF;
    gdt_set_gate(8, 0x1000, max_limit, 0x9A, 0xF0);
    ASSERT(gdt[8].limit_low == 0xFFFF);
    ASSERT((gdt[8].granularity & 0x0F) == 0x0F);

    // Test limit overflow (should be truncated to 20 bits)
    uint32_t overflow_limit = 0x123456;
    gdt_set_gate(8, 0x1000, overflow_limit, 0x9A, 0);
    ASSERT(gdt[8].limit_low == (overflow_limit & 0xFFFF));
    ASSERT((gdt[8].granularity & 0x0F) == ((overflow_limit >> 16) & 0x0F));

    gdt[8] = original;
}

// Test granularity and operand size flags
TEST(gdt_granularity_flags)
{
    gdt_descriptor_t original = gdt[9];

    // Test with granularity bit set (4KB pages)
    gdt_set_gate(9, 0, 0x1000, 0x9A, GDT_GRANULARITY);
    ASSERT(gdt[9].granularity & GDT_GRANULARITY);

    // Test with operand size bit set (32-bit)
    gdt_set_gate(9, 0, 0x1000, 0x9A, GDT_OPERAND_SIZE);
    ASSERT(gdt[9].granularity & GDT_OPERAND_SIZE);

    // Test with both flags
    gdt_set_gate(9, 0, 0x1000, 0x9A, GDT_GRANULARITY | GDT_OPERAND_SIZE);
    ASSERT(gdt[9].granularity & GDT_GRANULARITY);
    ASSERT(gdt[9].granularity & GDT_OPERAND_SIZE);

    gdt[9] = original;
}

// Test access bit combinations
TEST(gdt_access_combinations)
{
    gdt_descriptor_t original = gdt[6];

    // Test present + kernel + code + readable
    uint8_t access = GDT_PRESENT | GDT_KERNEL | GDT_CODE | GDT_RW;
    gdt_set_gate(6, 0, 0x1000, access, 0);
    ASSERT(gdt[6].access == access);
    ASSERT(gdt[6].access & GDT_PRESENT);
    ASSERT(!(gdt[6].access & GDT_USER)); // Should be kernel mode
    ASSERT(gdt[6].access & GDT_EX);      // Should be executable
    ASSERT(gdt[6].access & GDT_RW);      // Should be readable

    // Test present + user + data + writable
    access = GDT_PRESENT | GDT_USER | GDT_DATA;
    gdt_set_gate(6, 0, 0x1000, access, 0);
    ASSERT(gdt[6].access == access);
    ASSERT(gdt[6].access & GDT_PRESENT);
    ASSERT(gdt[6].access & GDT_USER);  // Should be user mode
    ASSERT(!(gdt[6].access & GDT_EX)); // Should not be executable
    ASSERT(gdt[6].access & GDT_RW);    // Should be writable (for data)

    gdt[6] = original;
}
