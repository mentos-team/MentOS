/// @file runner.c
/// @brief Kernel test runner.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "tests/test.h"

/// @brief Test function pointer type.
typedef void (*test_func_t)(void);

/// @brief Test entry structure.
typedef struct {
    test_func_t func;
    const char *name;
} test_entry_t;

/// @brief Centralized list of all kernel tests using X-macro pattern
/// To add a new test:
/// 1. Add X(test_name) to the TEST_LIST macro below
/// 2. Implement TEST(test_name) in the appropriate test file
///
/// The X-macro pattern automatically generates forward declarations
/// and test registry entries in runner.c
#define TEST_LIST                  \
    X(gdt_set_gate)                \
    X(gdt_bounds_check)            \
    X(gdt_segment_types)           \
    X(gdt_base_address_fields)     \
    X(gdt_limit_fields)            \
    X(gdt_granularity_composition) \
    X(gdt_null_descriptor)         \
    X(gdt_initialization_state)    \
    X(gdt_privilege_levels)        \
    X(gdt_segment_flags)           \
    X(gdt_limit_boundaries)        \
    X(gdt_granularity_flags)       \
    X(gdt_access_combinations)     \
    X(idt_initialization)          \
    X(idt_bounds_check)            \
    X(idt_gate_types)              \
    X(idt_privilege_levels)        \
    X(idt_segment_selectors)       \
    X(idt_present_bits)            \
    X(idt_reserved_fields)         \
    X(idt_offset_fields)           \
    X(idt_table_size)              \
    X(idt_interrupt_ranges)        \
    X(idt_options_composition)     \
    X(isr_install_handler)         \
    X(isr_bounds_check)            \
    X(isr_uninstall_handler)       \
    X(isr_uninstall_bounds_check)  \
    X(isr_default_handlers)        \
    X(isr_arrays_initialization)   \
    X(exception_messages)          \
    X(isr_handler_replacement)     \
    X(isr_multiple_handlers)       \
    X(irq_initialization)          \
    X(irq_install_handler)         \
    X(irq_bounds_check)            \
    X(irq_multiple_handlers)       \
    X(irq_uninstall_handler)       \
    X(irq_uninstall_bounds_check)  \
    X(irq_uninstall_nonexistent)   \
    X(irq_all_lines)               \
    X(irq_constants)               \
    X(irq_null_parameters)

/// @brief Create a test entry for the test registry.
/// @param name The name of the test.
#define TEST_ENTRY(name)   \
    {                      \
        test_##name, #name \
    }

// Auto-generate forward declarations
#define X(name) TEST(name);
TEST_LIST
#undef X

// Auto-generate test registry
static const test_entry_t test_functions[] = {
#define X(name) TEST_ENTRY(name),
    TEST_LIST
#undef X
};
static const int num_tests = sizeof(test_functions) / sizeof(test_entry_t);

/// @brief Run all kernel tests.
/// @return 0 on success, -1 on failure.
int kernel_run_tests(void)
{
    pr_info("Starting kernel tests...\n");
    int passed = 0;
    for (int i = 0; i < num_tests; i++) {
        pr_info("Test %d/%d: %s\n", i + 1, num_tests, test_functions[i].name);
        test_functions[i].func();
        passed++;
    }
    pr_info("Kernel tests completed: %d/%d passed\n", passed, num_tests);

    return (passed == num_tests) ? 0 : -1;
}
