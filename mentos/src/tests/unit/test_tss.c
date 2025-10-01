/// @file test_tss.c
/// @brief Unit tests for TSS (Task State Segment) functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "descriptor_tables/gdt.h"
#include "descriptor_tables/tss.h"
#include "tests/test.h"

// Test TSS initialization
TEST(tss_initialization)
{
    // Verify the TSS structure was initialized
    extern tss_entry_t kernel_tss;
    ASSERT(kernel_tss.ss0 == 0x10);
    ASSERT(kernel_tss.esp0 == 0x0);
    ASSERT(kernel_tss.cs == 0x0b);
    ASSERT(kernel_tss.ds == 0x13);
    ASSERT(kernel_tss.es == 0x13);
    ASSERT(kernel_tss.fs == 0x13);
    ASSERT(kernel_tss.gs == 0x13);
    ASSERT(kernel_tss.ss == 0x13);
    ASSERT(kernel_tss.iomap == sizeof(tss_entry_t));
}

// Test TSS structure size
TEST(tss_structure_size)
{
    // Verify TSS structure size is correct (104 bytes for x86)
    ASSERT(sizeof(tss_entry_t) == 104);
}

// Test TSS initialization clears unused fields (tests final state after all operations)
TEST(tss_unused_fields_cleared)
{
    // TSS should be in a known state from previous tests
    extern tss_entry_t kernel_tss;

    // Verify unused fields are cleared (from memset in tss_init)
    ASSERT(kernel_tss.prev_tss == 0x0);
    ASSERT(kernel_tss.esp1 == 0x0);
    ASSERT(kernel_tss.ss1 == 0x0);
    ASSERT(kernel_tss.esp2 == 0x0);
    ASSERT(kernel_tss.ss2 == 0x0);
    ASSERT(kernel_tss.cr3 == 0x0);
    ASSERT(kernel_tss.eip == 0x0);
    ASSERT(kernel_tss.eflags == 0x0);
    ASSERT(kernel_tss.eax == 0x0);
    ASSERT(kernel_tss.ecx == 0x0);
    ASSERT(kernel_tss.edx == 0x0);
    ASSERT(kernel_tss.ebx == 0x0);
    ASSERT(kernel_tss.esp == 0x0);
    ASSERT(kernel_tss.ebp == 0x0);
    ASSERT(kernel_tss.esi == 0x0);
    ASSERT(kernel_tss.edi == 0x0);
    // Note: es is set to 0x13, not cleared
    ASSERT(kernel_tss.ldt == 0x0);
    ASSERT(kernel_tss.trap == 0x0);
}
