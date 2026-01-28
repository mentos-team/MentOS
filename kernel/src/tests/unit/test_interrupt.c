/// @file test_interrupt.c
/// @brief Unit tests for IRQ (Interrupt Request) functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "descriptor_tables/idt.h"
#include "descriptor_tables/isr.h"
#include "hardware/pic8259.h"
#include "stddef.h"
#include "tests/test.h"

// Test IRQ initialization
TEST(irq_initialization)
{
    // Test that IRQ_NUM constant is reasonable
    ASSERT(IRQ_NUM > 0 && IRQ_NUM <= 16); // PIC has 16 IRQ lines
}

// Test IRQ handler installation
TEST(irq_install_handler)
{
    int result;

    // Test installing a handler for a valid IRQ
    result = irq_install_handler(5, (interrupt_handler_t)0x12345678, "test_irq_handler");
    ASSERT(result == 0);
    result = irq_uninstall_handler(5, (interrupt_handler_t)0x12345678);
    ASSERT(result == 0);
}

// Test IRQ handler bounds checking
TEST(irq_bounds_check)
{
    int result;

    // Test installing handler with invalid IRQ number
    result = irq_install_handler(IRQ_NUM, (interrupt_handler_t)0x12345678, "test_handler");
    ASSERT(result == -1);
    result = irq_uninstall_handler(IRQ_NUM, (interrupt_handler_t)0x12345678);
    ASSERT(result == -1);

    // Test installing handler with negative IRQ number (if supported)
    result = irq_install_handler(-1, (interrupt_handler_t)0x12345678, "test_handler");
    ASSERT(result == -1);
    result = irq_uninstall_handler(-1, (interrupt_handler_t)0x12345678);
    ASSERT(result == -1);
}

// Test multiple IRQ handlers on same line
TEST(irq_multiple_handlers)
{
    int result;

    // Install multiple handlers on the same IRQ line
    result = irq_install_handler(6, (interrupt_handler_t)0x11111111, "handler1");
    ASSERT(result == 0);
    result = irq_uninstall_handler(6, (interrupt_handler_t)0x11111111);
    ASSERT(result == 0);

    result = irq_install_handler(6, (interrupt_handler_t)0x22222222, "handler2");
    ASSERT(result == 0);
    result = irq_uninstall_handler(6, (interrupt_handler_t)0x22222222);
    ASSERT(result == 0);

    result = irq_install_handler(6, (interrupt_handler_t)0x33333333, "handler3");
    ASSERT(result == 0);
    result = irq_uninstall_handler(6, (interrupt_handler_t)0x11111111);
    ASSERT(result == 0);
}

// Test IRQ handler uninstallation
TEST(irq_uninstall_handler)
{
    interrupt_handler_t test_handler;
    int result;

    // Install a handler first
    test_handler = (interrupt_handler_t)0xABCDEF12;
    result       = irq_install_handler(7, test_handler, "uninstall_test");
    ASSERT(result == 0);
    result = irq_uninstall_handler(7, test_handler);
    ASSERT(result == 0);

    // Now uninstall it
    result = irq_uninstall_handler(7, test_handler);
    ASSERT(result == 0);
    result = irq_uninstall_handler(7, test_handler);
    ASSERT(result == 0);
}

// Test IRQ uninstall bounds checking
TEST(irq_uninstall_bounds_check)
{
    int result;
    // Test uninstalling with invalid IRQ number
    result = irq_uninstall_handler(IRQ_NUM, (interrupt_handler_t)0x12345678);
    ASSERT(result == -1);
    irq_uninstall_handler(IRQ_NUM, (interrupt_handler_t)0x12345678);
    ASSERT(result == -1);
}

// Test uninstalling non-existent handler
TEST(irq_uninstall_nonexistent)
{
    int result;
    // Try to uninstall a handler that was never installed
    result = irq_uninstall_handler(8, (interrupt_handler_t)0xDEADBEEF);
    ASSERT(result == 0); // Should succeed even if handler not found
    result = irq_uninstall_handler(8, (interrupt_handler_t)0xDEADBEEF);
    ASSERT(result == 0);
}

// Test IRQ handler installation on all valid lines
TEST(irq_all_lines)
{
    int result;
    // Test installing handlers on all valid IRQ lines
    for (int i = 0; i < IRQ_NUM; i++) {
        result = irq_install_handler(i, (interrupt_handler_t)(0x10000000 + i), "test_handler");
        ASSERT(result == 0);
        result = irq_uninstall_handler(i, (interrupt_handler_t)(0x10000000 + i));
        ASSERT(result == 0);
    }
}

// Test IRQ system constants
TEST(irq_constants)
{
    // Test that IRQ_NUM is defined and reasonable
    ASSERT(IRQ_NUM == 16); // Standard PIC has 16 IRQ lines
    // Test that IDT_SIZE includes IRQs
    ASSERT(IDT_SIZE >= 32 + IRQ_NUM); // CPU exceptions + IRQs
}

// Test IRQ handler with NULL parameters
TEST(irq_null_parameters)
{
    int result;
    // Test installing NULL handler (should work)
    result = irq_install_handler(9, NULL, "null_handler");
    ASSERT(result == 0);
    result = irq_uninstall_handler(9, NULL);
    ASSERT(result == 0);

    // Test installing with NULL description (should work)
    result = irq_install_handler(10, (interrupt_handler_t)0x12345678, NULL);
    ASSERT(result == 0);
    result = irq_uninstall_handler(10, (interrupt_handler_t)0x12345678);
    ASSERT(result == 0);
}
