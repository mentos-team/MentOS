/// @file test_idt.c
/// @brief IDT unit tests - Non-destructive version.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "descriptor_tables/gdt.h"
#include "descriptor_tables/idt.h"
#include "string.h"
#include "tests/test.h"
#include "tests/test_utils.h"

// External declaration for IDT table and pointer
extern idt_descriptor_t idt_table[IDT_SIZE];
extern idt_pointer_t idt_pointer;

/// @brief Safe IDT entry copy for testing (read-only access).
/// @param src_idx Source IDT index.
/// @param dest_buffer Destination buffer (must be at least 8 bytes).
/// @return 0 on success, -1 on invalid index.
static inline int idt_safe_copy(size_t src_idx, void *dest_buffer)
{
    if (src_idx >= IDT_SIZE) {
        pr_warning("Invalid IDT index %zu (max: %d)\n", src_idx, IDT_SIZE - 1);
        return -1;
    }
    if (dest_buffer == NULL) {
        pr_warning("NULL destination buffer for IDT copy\n");
        return -1;
    }
    memcpy(dest_buffer, &idt_table[src_idx], sizeof(idt_descriptor_t));
    return 0;
}

/// @brief Test that the IDT structure has the correct size.
TEST(idt_structure_size)
{
    TEST_SECTION_START("IDT structure size");
    ASSERT(sizeof(idt_descriptor_t) == 8);
    TEST_SECTION_END();
}

/// @brief Verify IDT pointer configuration.
TEST(idt_pointer_configuration)
{
    TEST_SECTION_START("IDT pointer configuration");

    ASSERT_MSG((uint32_t)&idt_table == idt_pointer.base, "IDT pointer base must point to IDT table");

    uint16_t expected_limit = sizeof(idt_descriptor_t) * IDT_SIZE - 1;
    ASSERT_MSG(idt_pointer.limit == expected_limit, "IDT pointer limit must be size-1");

    TEST_SECTION_END();
}

/// @brief Verify IDT reserved field is zero for all entries.
TEST(idt_reserved_field_zero)
{
    TEST_SECTION_START("IDT reserved field zero");

    for (int i = 0; i < IDT_SIZE; i++) {
        idt_descriptor_t entry;
        ASSERT(idt_safe_copy(i, &entry) == 0);
        ASSERT_MSG(entry.reserved == 0, "IDT reserved field must be zero");
    }

    TEST_SECTION_END();
}

/// @brief Verify exception and IRQ entries are present and correctly configured.
TEST(idt_exception_irq_entries)
{
    TEST_SECTION_START("IDT exception/IRQ entries");

    for (int i = 0; i <= 47; i++) {
        idt_descriptor_t entry;
        ASSERT(idt_safe_copy(i, &entry) == 0);

        uint32_t offset = entry.offset_low | ((uint32_t)entry.offset_high << 16);
        ASSERT_MSG(offset != 0, "IDT handler offset must be non-zero");

        ASSERT_MSG((entry.options & GDT_PRESENT) != 0, "IDT entry must be present");
        ASSERT_MSG((entry.options & 0x60) == GDT_KERNEL, "IDT entry DPL must be 0 for kernel");
        ASSERT_MSG((entry.options & 0x0F) == IDT_PADDING, "IDT entry type must be 32-bit interrupt gate");
        ASSERT_MSG(entry.seg_selector == 0x8, "IDT segment selector must be 0x08");
    }

    TEST_SECTION_END();
}

/// @brief Verify system call entry (0x80) is user accessible and configured.
TEST(idt_syscall_entry)
{
    TEST_SECTION_START("IDT syscall entry");

    idt_descriptor_t entry;
    ASSERT(idt_safe_copy(0x80, &entry) == 0);

    uint32_t offset = entry.offset_low | ((uint32_t)entry.offset_high << 16);
    ASSERT_MSG(offset != 0, "Syscall handler offset must be non-zero");

    ASSERT_MSG((entry.options & GDT_PRESENT) != 0, "Syscall entry must be present");
    ASSERT_MSG((entry.options & 0x60) == GDT_USER, "Syscall entry DPL must be 3");
    ASSERT_MSG((entry.options & 0x0F) == IDT_PADDING, "Syscall entry type must be 32-bit interrupt gate");
    ASSERT_MSG(entry.seg_selector == 0x8, "Syscall segment selector must be 0x08");

    TEST_SECTION_END();
}

/// @brief Verify unused IDT entries remain zeroed.
TEST(idt_unused_entries_zeroed)
{
    TEST_SECTION_START("IDT unused entries zeroed");

    for (int i = 48; i < IDT_SIZE; i++) {
        if (i == 0x80) {
            continue;
        }
        idt_descriptor_t entry;
        ASSERT(idt_safe_copy(i, &entry) == 0);
        ASSERT_MSG(test_is_zeroed(&entry, sizeof(entry), "unused_idt_entry"), "Unused IDT entry must be zeroed");
    }

    TEST_SECTION_END();
}

/// @brief Main test function for IDT subsystem.
/// This function runs all IDT tests in sequence.
void test_idt(void)
{
    test_idt_structure_size();
    test_idt_pointer_configuration();
    test_idt_reserved_field_zero();
    test_idt_exception_irq_entries();
    test_idt_syscall_entry();
    test_idt_unused_entries_zeroed();
}
