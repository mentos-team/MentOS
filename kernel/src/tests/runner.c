/// @file runner.c
/// @brief Kernel test runner.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "tests/test.h"

/// @brief Test function pointer type.
typedef void (*test_func_t)(void);

/// @brief Test entry structure.
typedef struct {
    test_func_t func;
    const char *name;
} test_entry_t;

/// @brief Forward declarations for all test suite functions.
/// @note To add a new test suite:
///       1. Create a test file (e.g., test_idt.c)
///       2. Implement individual tests in that file
///       3. Add a test_idt(void) that calls them all
///       4. Add extern declaration below
///       5. Add one entry to test_functions array

extern void test_gdt(void);
extern void test_idt(void);

/// @brief Test registry - one entry per subsystem.
static const test_entry_t test_functions[] = {
    {test_gdt, "GDT Subsystem"},
    {test_idt, "IDT Subsystem"},
};

static const int num_tests = sizeof(test_functions) / sizeof(test_entry_t);

/// @brief Run all kernel tests.
/// @return 0 on success, -1 on failure.
int kernel_run_tests(void)
{
    pr_notice("Starting kernel tests...\n");
    int passed = 0;
    for (int i = 0; i < num_tests; i++) {
        pr_notice("========== %s ==========\n", test_functions[i].name);
        test_functions[i].func();
        passed++;
        pr_notice("========== %s Done ==========\n", test_functions[i].name);
    }
    pr_notice("Kernel tests completed: %d/%d passed\n", passed, num_tests);

    return (passed == num_tests) ? 0 : -1;
}
