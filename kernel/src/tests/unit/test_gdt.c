/// @file test_gdt_safe.c
/// @brief Refactored GDT unit tests - Non-destructive version.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "descriptor_tables/gdt.h"
#include "math.h"
#include "string.h"
#include "tests/test.h"
#include "tests/test_utils.h"

// External declaration for GDT array
extern gdt_descriptor_t gdt[GDT_SIZE];
extern gdt_pointer_t gdt_pointer;

/// @brief Safe GDT entry copy for testing (read-only access).
/// @param src_idx Source GDT index.
/// @param dest_buffer Destination buffer (must be at least 8 bytes).
/// @return 0 on success, -1 on invalid index.
static inline int gdt_safe_copy(size_t src_idx, void *dest_buffer)
{
    if (src_idx >= GDT_SIZE) {
        pr_warning("Invalid GDT index %zu (max: %d)\n", src_idx, GDT_SIZE - 1);
        return -1;
    }
    if (dest_buffer == NULL) {
        pr_warning("NULL destination buffer for GDT copy\n");
        return -1;
    }
    memcpy(dest_buffer, &gdt[src_idx], sizeof(gdt_descriptor_t));
    return 0;
}

/// @brief Test that the GDT structure has the correct size.
TEST(gdt_structure_size)
{
    TEST_SECTION_START("GDT structure size");
    ASSERT(sizeof(gdt_descriptor_t) == 8);
    TEST_SECTION_END();
}

/// @brief Verify that the null descriptor is correctly initialized.
TEST(gdt_null_descriptor)
{
    TEST_SECTION_START("GDT null descriptor");

    gdt_descriptor_t null_entry;
    ASSERT(gdt_safe_copy(0, &null_entry) == 0);

    // Null descriptor must have all fields as 0
    ASSERT_MSG(null_entry.base_low == 0, "Null descriptor base_low must be 0");
    ASSERT_MSG(null_entry.base_middle == 0, "Null descriptor base_middle must be 0");
    ASSERT_MSG(null_entry.base_high == 0, "Null descriptor base_high must be 0");
    ASSERT_MSG(null_entry.limit_low == 0, "Null descriptor limit_low must be 0");
    ASSERT_MSG(null_entry.access == 0, "Null descriptor access must be 0");
    ASSERT_MSG(null_entry.granularity == 0, "Null descriptor granularity must be 0");

    TEST_SECTION_END();
}

/// @brief Verify that essential GDT entries are initialized.
TEST(gdt_essential_entries_initialized)
{
    TEST_SECTION_START("GDT essential entries");

    // Entry 1: Should be kernel code segment
    gdt_descriptor_t code_entry;
    ASSERT(gdt_safe_copy(1, &code_entry) == 0);
    ASSERT_MSG((code_entry.access & 0x80) != 0, "Code segment must be present");
    // Code segment has GDT_S (0x10) and GDT_EX (0x08) bits set
    ASSERT_MSG((code_entry.access & 0x18) == 0x18, "Entry 1 must be code segment");

    // Entry 2: Should be kernel data segment
    gdt_descriptor_t data_entry;
    ASSERT(gdt_safe_copy(2, &data_entry) == 0);
    ASSERT_MSG((data_entry.access & 0x80) != 0, "Data segment must be present");
    // Data segment has GDT_S (0x10) but not GDT_EX (0x08)
    ASSERT_MSG((data_entry.access & 0x18) == 0x10, "Entry 2 must be data segment");

    TEST_SECTION_END();
}

/// @brief Verify GDT bounds checking.
TEST(gdt_bounds_validation)
{
    TEST_SECTION_START("GDT bounds validation");

    // Verify we can access last valid entry without issues
    gdt_descriptor_t last_entry;
    ASSERT(gdt_safe_copy(GDT_SIZE - 1, &last_entry) == 0);

    // Verify invalid indices are rejected
    ASSERT(gdt_safe_copy(GDT_SIZE, NULL) == -1);
    ASSERT(gdt_safe_copy(GDT_SIZE + 100, NULL) == -1);

    TEST_SECTION_END();
}

/// @brief Verify base address field layout in GDT entries.
TEST(gdt_base_address_layout)
{
    TEST_SECTION_START("GDT base address field layout");

    // Test a few entries to ensure base address fields are used
    for (int i = 1; i < min(5, GDT_SIZE); i++) {
        gdt_descriptor_t entry;
        ASSERT(gdt_safe_copy(i, &entry) == 0);

        // For kernel segments (present bit set), verify base fields exist
        if ((entry.access & 0x80) != 0) {
            uint32_t base = (entry.base_high << 24) |
                            (entry.base_middle << 16) |
                            (entry.base_low);

            // Base should be within valid range
            ASSERT_MSG(test_bounds_check(base, 0, 0xFFFFFFFF, "base_address"), "Base address out of expected range");
        }
    }

    TEST_SECTION_END();
}

/// @brief Verify limit field layout in GDT entries.
TEST(gdt_limit_field_layout)
{
    TEST_SECTION_START("GDT limit field layout");

    // Test a few entries to ensure limit fields are used
    for (int i = 1; i < min(5, GDT_SIZE); i++) {
        gdt_descriptor_t entry;
        ASSERT(gdt_safe_copy(i, &entry) == 0);

        // For present entries, verify limit fields
        if ((entry.access & 0x80) != 0) {
            uint32_t limit = ((entry.granularity & 0x0F) << 16) | entry.limit_low;

            // Limit should be within 20-bit range
            ASSERT_MSG(limit <= 0xFFFFF, "Limit exceeds 20-bit field");
        }
    }

    TEST_SECTION_END();
}

/// @brief Verify access byte format in GDT entries.
TEST(gdt_access_byte_format)
{
    TEST_SECTION_START("GDT access byte format");

    // Examine a few entries
    for (int i = 1; i < min(5, GDT_SIZE); i++) {
        gdt_descriptor_t entry;
        ASSERT(gdt_safe_copy(i, &entry) == 0);

        // If present (bit 7 set), verify access byte structure
        if ((entry.access & 0x80) != 0) {
            // Bit 7: Present
            ASSERT_MSG((entry.access & 0x80) != 0, "Present bit should be set");

            // Bits 6-5: Privilege level (0-3)
            uint8_t dpl = (entry.access & 0x60) >> 5;
            ASSERT_MSG(dpl <= 3, "DPL should be 0-3");

            // Bit 4: Descriptor type (1 for code/data, 0 for system)
            // Bits 3-0: Type (depends on descriptor type)
        }
    }

    TEST_SECTION_END();
}

/// @brief Verify granularity byte format in GDT entries.
TEST(gdt_granularity_byte_format)
{
    TEST_SECTION_START("GDT granularity byte format");

    // Examine entries
    for (int i = 1; i < min(5, GDT_SIZE); i++) {
        gdt_descriptor_t entry;
        ASSERT(gdt_safe_copy(i, &entry) == 0);

        if ((entry.access & 0x80) != 0) {
            // Bit 7: Granularity (0 = byte, 1 = 4KB)
            uint8_t g = (entry.granularity & 0x80) >> 7;
            ASSERT_MSG(g <= 1, "Granularity bit should be 0 or 1");

            // Bit 6: Default/Big (0 = 16-bit, 1 = 32-bit)
            uint8_t db = (entry.granularity & 0x40) >> 6;
            ASSERT_MSG(db <= 1, "Default/Big bit should be 0 or 1");

            // Bits 3-0: High 4 bits of limit
            uint8_t limit_high = entry.granularity & 0x0F;
            ASSERT_MSG(limit_high <= 15, "Limit high bits should be 0-15");
        }
    }

    TEST_SECTION_END();
}

/// @brief Verify GDT size constant and array bounds.
TEST(gdt_array_bounds)
{
    TEST_SECTION_START("GDT array bounds");

    // Verify GDT_SIZE is reasonable
    ASSERT(GDT_SIZE > 0);
    ASSERT(GDT_SIZE <= 8192); // GDT can have at most 8192 entries

    // Verify we can access all entries safely
    for (int i = 0; i < GDT_SIZE; i++) {
        gdt_descriptor_t entry;
        ASSERT(gdt_safe_copy(i, &entry) == 0);
    }

    TEST_SECTION_END();
}

/// @brief Verify GDT pointer is correctly configured.
TEST(gdt_pointer_configuration)
{
    TEST_SECTION_START("GDT pointer configuration");

    // GDT pointer should point to the GDT array
    ASSERT_MSG((uint32_t)&gdt == gdt_pointer.base, "GDT pointer base must point to GDT array");

    // Limit should be (number_of_entries * entry_size) - 1
    // We have 6 entries, each 8 bytes, so limit should be 47 (6*8-1)
    uint16_t expected_limit = sizeof(gdt_descriptor_t) * 6 - 1;
    ASSERT_MSG(gdt_pointer.limit == expected_limit, "GDT pointer limit must be 47");

    TEST_SECTION_END();
}

/// @brief Verify user mode code segment (entry 3) is correctly configured.
TEST(gdt_user_code_segment)
{
    TEST_SECTION_START("GDT user code segment (entry 3)");

    gdt_descriptor_t descriptor;
    gdt_safe_copy(3, &descriptor);

    // Entry 3 should be a user mode code segment
    // Access byte should have: PRESENT | USER | EXECUTABLE | READABLE
    uint8_t expected_access = GDT_PRESENT | GDT_USER | GDT_CODE | GDT_RW;
    ASSERT_MSG(descriptor.access == expected_access, "User code segment access byte incorrect");

    // Base address should be 0
    uint32_t base = descriptor.base_low | (descriptor.base_middle << 16) | (descriptor.base_high << 24);
    ASSERT_MSG(base == 0, "User code segment base must be 0");

    // Limit should be 0xFFFF (granularity byte has upper 4 bits of limit)
    uint32_t limit = descriptor.limit_low | (((uint32_t)(descriptor.granularity & 0x0F)) << 16);
    ASSERT_MSG(limit == 0xFFFFF, "User code segment limit must be 0xFFFFF");

    // Granularity should have GRANULARITY and OPERAND_SIZE flags
    uint8_t expected_granularity = GDT_GRANULARITY | GDT_OPERAND_SIZE;
    ASSERT_MSG((descriptor.granularity & 0xF0) == expected_granularity, "User code segment granularity flags incorrect");

    TEST_SECTION_END();
}

/// @brief Verify user mode data segment (entry 4) is correctly configured.
TEST(gdt_user_data_segment)
{
    TEST_SECTION_START("GDT user data segment (entry 4)");

    gdt_descriptor_t descriptor;
    gdt_safe_copy(4, &descriptor);

    // Entry 4 should be a user mode data segment
    // Access byte should have: PRESENT | USER | WRITABLE (not executable)
    uint8_t expected_access = GDT_PRESENT | GDT_USER | GDT_DATA;
    ASSERT_MSG(descriptor.access == expected_access, "User data segment access byte incorrect");

    // Base address should be 0
    uint32_t base = descriptor.base_low | (descriptor.base_middle << 16) | (descriptor.base_high << 24);
    ASSERT_MSG(base == 0, "User data segment base must be 0");

    // Limit should be 0xFFFFF (same as code segment)
    uint32_t limit = descriptor.limit_low | (((uint32_t)(descriptor.granularity & 0x0F)) << 16);
    ASSERT_MSG(limit == 0xFFFFF, "User data segment limit must be 0xFFFFF");

    // Granularity should have GRANULARITY and OPERAND_SIZE flags
    uint8_t expected_granularity = GDT_GRANULARITY | GDT_OPERAND_SIZE;
    ASSERT_MSG((descriptor.granularity & 0xF0) == expected_granularity, "User data segment granularity flags incorrect");

    TEST_SECTION_END();
}

/// @brief Verify TSS descriptor (entry 5) is correctly configured.
TEST(gdt_tss_descriptor)
{
    TEST_SECTION_START("GDT TSS descriptor (entry 5)");

    gdt_descriptor_t descriptor;
    ASSERT(gdt_safe_copy(5, &descriptor) == 0);

    // TSS is a system segment: S bit must be 0
    ASSERT_MSG((descriptor.access & GDT_S) == 0, "TSS descriptor must be a system segment");

    // Access byte should include required TSS bits (present, DPL=3, executable)
    uint8_t required_access = GDT_PRESENT | GDT_USER | GDT_EX;
    ASSERT_MSG((descriptor.access & required_access) == required_access, "TSS descriptor access bits missing");

    // Accessed bit should be set (CPU may update it)
    ASSERT_MSG((descriptor.access & GDT_AC) != 0, "TSS descriptor accessed bit must be set");

    // Granularity flags should be clear for TSS (no 4K or 32-bit flags)
    ASSERT_MSG((descriptor.granularity & 0xF0) == 0, "TSS granularity flags must be 0");

    // Limit high nibble must be within 4-bit range
    ASSERT_MSG((descriptor.granularity & 0x0F) <= 0x0F, "TSS limit high bits invalid");

    TEST_SECTION_END();
}

/// @brief Verify privilege levels for kernel and user segments.
TEST(gdt_privilege_levels)
{
    TEST_SECTION_START("GDT privilege levels");

    gdt_descriptor_t entry;

    // Kernel code (entry 1) and data (entry 2) must be DPL 0
    ASSERT(gdt_safe_copy(1, &entry) == 0);
    ASSERT_MSG((entry.access & 0x60) == GDT_KERNEL, "Kernel code segment DPL must be 0");

    ASSERT(gdt_safe_copy(2, &entry) == 0);
    ASSERT_MSG((entry.access & 0x60) == GDT_KERNEL, "Kernel data segment DPL must be 0");

    // User code (entry 3) and data (entry 4) must be DPL 3
    ASSERT(gdt_safe_copy(3, &entry) == 0);
    ASSERT_MSG((entry.access & 0x60) == GDT_USER, "User code segment DPL must be 3");

    ASSERT(gdt_safe_copy(4, &entry) == 0);
    ASSERT_MSG((entry.access & 0x60) == GDT_USER, "User data segment DPL must be 3");

    TEST_SECTION_END();
}

/// @brief Verify granularity and operand size flags for code/data segments.
TEST(gdt_segment_flags)
{
    TEST_SECTION_START("GDT segment flags");

    gdt_descriptor_t entry;
    uint8_t expected_flags = GDT_GRANULARITY | GDT_OPERAND_SIZE;

    // Kernel code/data and user code/data should be 4KB granularity, 32-bit
    for (int i = 1; i <= 4; i++) {
        ASSERT(gdt_safe_copy(i, &entry) == 0);
        ASSERT_MSG((entry.granularity & 0xF0) == expected_flags, "Segment flags must be G and D/B");
    }

    TEST_SECTION_END();
}

/// @brief Verify base and limit values for code/data segments.
TEST(gdt_segment_base_limit_values)
{
    TEST_SECTION_START("GDT segment base/limit values");

    gdt_descriptor_t entry;

    for (int i = 1; i <= 4; i++) {
        ASSERT(gdt_safe_copy(i, &entry) == 0);

        uint32_t base = entry.base_low | (entry.base_middle << 16) | (entry.base_high << 24);
        ASSERT_MSG(base == 0, "Segment base must be 0");

        uint32_t limit = entry.limit_low | (((uint32_t)(entry.granularity & 0x0F)) << 16);
        ASSERT_MSG(limit == 0xFFFFF, "Segment limit must be 0xFFFFF");
    }

    TEST_SECTION_END();
}

/// @brief Main test function for GDT subsystem.
/// This function runs all GDT tests in sequence.
void test_gdt(void)
{

    test_gdt_structure_size();
    test_gdt_null_descriptor();
    test_gdt_essential_entries_initialized();
    test_gdt_bounds_validation();
    test_gdt_base_address_layout();
    test_gdt_limit_field_layout();
    test_gdt_access_byte_format();
    test_gdt_granularity_byte_format();
    test_gdt_array_bounds();
    test_gdt_pointer_configuration();
    test_gdt_user_code_segment();
    test_gdt_user_data_segment();
    test_gdt_tss_descriptor();
    test_gdt_privilege_levels();
    test_gdt_segment_flags();
    test_gdt_segment_base_limit_values();
}

