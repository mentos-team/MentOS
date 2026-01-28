/// @file test_exception.c
/// @brief Unit tests for exception handling and ISR functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "descriptor_tables/idt.h"
#include "descriptor_tables/isr.h"
#include "stddef.h"
#include "string.h"
#include "tests/test.h"

// Extern declarations for ISR arrays
extern interrupt_handler_t isr_routines[IDT_SIZE];
extern char *isr_routines_description[IDT_SIZE];

// Test ISR handler installation
TEST(isr_install_handler)
{
    // Test installing a handler for a valid interrupt
    int result = isr_install_handler(50, (interrupt_handler_t)0x12345678, "test_handler");
    ASSERT(result == 0);
    ASSERT(isr_routines[50] == (interrupt_handler_t)0x12345678);
    ASSERT(strcmp(isr_routines_description[50], "test_handler") == 0);

    // Clean up
    isr_uninstall_handler(50);
}

// Test ISR handler bounds checking
TEST(isr_bounds_check)
{
    // Test installing handler with invalid interrupt number
    int result = isr_install_handler(IDT_SIZE, (interrupt_handler_t)0x12345678, "test_handler");
    ASSERT(result == -1);

    // Test installing handler with maximum valid interrupt number
    result = isr_install_handler(IDT_SIZE - 1, (interrupt_handler_t)0x87654321, "max_handler");
    ASSERT(result == 0);
    ASSERT(isr_routines[IDT_SIZE - 1] == (interrupt_handler_t)0x87654321);

    // Clean up
    isr_uninstall_handler(IDT_SIZE - 1);
}

// Test ISR handler uninstallation
TEST(isr_uninstall_handler)
{
    // First install a handler
    isr_install_handler(51, (interrupt_handler_t)0xABCDEF12, "uninstall_test");
    ASSERT(isr_routines[51] == (interrupt_handler_t)0xABCDEF12);

    // Now uninstall it
    int result = isr_uninstall_handler(51);
    ASSERT(result == 0);
    // Should be reset to default handler (not our test handler)
    ASSERT(isr_routines[51] != (interrupt_handler_t)0xABCDEF12);
}

// Test ISR uninstall bounds checking
TEST(isr_uninstall_bounds_check)
{
    // Test uninstalling with invalid interrupt number
    int result = isr_uninstall_handler(IDT_SIZE);
    ASSERT(result == -1);
}

// Test default ISR handlers are installed
TEST(isr_default_handlers)
{
    // After initialization, all handlers should be set to default_isr_handler
    // We can't directly access default_isr_handler, but we can check it's not NULL
    for (int i = 0; i < 32; i++) { // CPU exceptions
        ASSERT(isr_routines[i] != NULL);
    }

    // Note: Descriptions are only set when handlers are explicitly installed,
    // so they may be NULL for default handlers. We don't test descriptions here.
}

// Test ISR arrays initialization
TEST(isr_arrays_initialization)
{
    // Test that ISR arrays are properly sized
    ASSERT(sizeof(isr_routines) == sizeof(interrupt_handler_t) * IDT_SIZE);
    ASSERT(sizeof(isr_routines_description) == sizeof(char *) * IDT_SIZE);

    // Test that arrays are accessible
    ASSERT(&isr_routines[0] != NULL);
    ASSERT(&isr_routines_description[0] != NULL);
    ASSERT(&isr_routines[IDT_SIZE - 1] != NULL);
    ASSERT(&isr_routines_description[IDT_SIZE - 1] != NULL);
}

// Test exception messages array
TEST(exception_messages)
{
    // Include the exception messages array
    extern const char *exception_messages[32];

    // Test that all exception messages are defined
    for (int i = 0; i < 32; i++) {
        ASSERT(exception_messages[i] != NULL);
        ASSERT(strlen(exception_messages[i]) > 0);
    }

    // Test specific known messages
    ASSERT(strcmp(exception_messages[0], "Division by zero") == 0);
    ASSERT(strcmp(exception_messages[13], "General protection fault") == 0);
    ASSERT(strcmp(exception_messages[14], "Page fault") == 0);
}

// Test ISR handler replacement and restoration
TEST(isr_handler_replacement)
{
    // Save original handler
    interrupt_handler_t original_handler = isr_routines[52];
    char *original_desc                  = isr_routines_description[52];

    // Install new handler
    isr_install_handler(52, (interrupt_handler_t)0xDEADBEEF, "replacement_test");
    ASSERT(isr_routines[52] == (interrupt_handler_t)0xDEADBEEF);
    ASSERT(strcmp(isr_routines_description[52], "replacement_test") == 0);

    // Replace with another handler
    isr_install_handler(52, (interrupt_handler_t)0xCAFEBABE, "another_test");
    ASSERT(isr_routines[52] == (interrupt_handler_t)0xCAFEBABE);
    ASSERT(strcmp(isr_routines_description[52], "another_test") == 0);

    // Restore original (uninstall)
    isr_uninstall_handler(52);
    ASSERT(isr_routines[52] != (interrupt_handler_t)0xCAFEBABE);
    // Note: we can't easily test restoration to exact original since default_isr_handler is static
}

// Test multiple ISR handlers
TEST(isr_multiple_handlers)
{
    // Install handlers for different interrupts
    isr_install_handler(53, (interrupt_handler_t)0x11111111, "handler1");
    isr_install_handler(54, (interrupt_handler_t)0x22222222, "handler2");
    isr_install_handler(55, (interrupt_handler_t)0x33333333, "handler3");

    // Verify they're all set correctly
    ASSERT(isr_routines[53] == (interrupt_handler_t)0x11111111);
    ASSERT(isr_routines[54] == (interrupt_handler_t)0x22222222);
    ASSERT(isr_routines[55] == (interrupt_handler_t)0x33333333);

    ASSERT(strcmp(isr_routines_description[53], "handler1") == 0);
    ASSERT(strcmp(isr_routines_description[54], "handler2") == 0);
    ASSERT(strcmp(isr_routines_description[55], "handler3") == 0);

    // Clean up
    isr_uninstall_handler(53);
    isr_uninstall_handler(54);
    isr_uninstall_handler(55);
}
