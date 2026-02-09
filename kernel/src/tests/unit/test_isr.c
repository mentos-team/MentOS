/// @file test_isr.c
/// @brief ISR unit tests - Non-destructive version.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "descriptor_tables/idt.h"
#include "descriptor_tables/isr.h"
#include "tests/test.h"
#include "tests/test_utils.h"

// External data from exception.c
extern interrupt_handler_t isr_routines[IDT_SIZE];
extern const char *exception_messages[32];

/// @brief Dummy handler for testing install/uninstall.
static void test_dummy_isr(pt_regs_t *frame)
{
    (void)frame;
}

/// @brief Verify ISR routines array is initialized.
TEST(isr_routines_initialized)
{
    TEST_SECTION_START("ISR routines initialized");

    for (int i = 0; i < IDT_SIZE; i++) {
        ASSERT_MSG(isr_routines[i] != NULL, "ISR routine must be non-null");
    }

    TEST_SECTION_END();
}

/// @brief Verify exception messages are present.
TEST(isr_exception_messages)
{
    TEST_SECTION_START("ISR exception messages");

    for (int i = 0; i < 32; i++) {
        ASSERT_MSG(exception_messages[i] != NULL, "Exception message must be non-null");
        ASSERT_MSG(exception_messages[i][0] != '\0', "Exception message must be non-empty");
    }

    TEST_SECTION_END();
}

/// @brief Verify ISR install/uninstall behavior.
TEST(isr_install_uninstall)
{
    TEST_SECTION_START("ISR install/uninstall");

    const unsigned test_index = 200;

    // Install handler
    ASSERT(isr_install_handler(test_index, test_dummy_isr, "test") == 0);
    ASSERT_MSG(isr_routines[test_index] == test_dummy_isr, "ISR handler must be installed");

    // Uninstall handler
    ASSERT(isr_uninstall_handler(test_index) == 0);
    ASSERT_MSG(isr_routines[test_index] != test_dummy_isr, "ISR handler must be uninstalled");

    TEST_SECTION_END();
}

/// @brief Verify ISR invalid index handling.
TEST(isr_invalid_index)
{
    TEST_SECTION_START("ISR invalid index");

    ASSERT(isr_install_handler(IDT_SIZE, test_dummy_isr, "bad") == -1);
    ASSERT(isr_uninstall_handler(IDT_SIZE) == -1);

    TEST_SECTION_END();
}

/// @brief Main test function for ISR subsystem.
/// This function runs all ISR tests in sequence.
void test_isr(void)
{
    test_isr_routines_initialized();
    test_isr_exception_messages();
    test_isr_install_uninstall();
    test_isr_invalid_index();
}
