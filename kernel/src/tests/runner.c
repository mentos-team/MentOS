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

/// @brief Forward declarations for all test suite functions.
/// @note To add a new test suite:
///       1. Create a test file (e.g., test_idt.c)
///       2. Implement individual tests in that file
///       3. Add a test_idt(void) that calls them all
///       4. Add extern declaration below
///       5. Add one entry to test_functions array

extern void test_gdt(void);
extern void test_idt(void);
extern void test_isr(void);
extern void test_paging(void);
extern void test_scheduler(void);
extern void test_zone_allocator(void);
extern void test_slab(void);
extern void test_vmem(void);
extern void test_mm(void);
extern void test_buddy(void);
extern void test_page(void);
extern void test_memory_adversarial(void);
extern void test_dma(void);

/// @brief Test registry - one entry per subsystem.
static const test_entry_t test_functions[] = {
    {test_gdt,                 "GDT Subsystem"                },
    {test_idt,                 "IDT Subsystem"                },
    {test_isr,                 "ISR Subsystem"                },
    {test_paging,              "Paging Subsystem"             },
    {test_scheduler,           "Scheduler Subsystem"          },
    {test_zone_allocator,      "Zone Allocator Subsystem"     },
    {test_slab,                "Slab Subsystem"               },
    {test_vmem,                "VMEM Subsystem"               },
    {test_mm,                  "MM/VMA Subsystem"             },
    {test_buddy,               "Buddy System Subsystem"       },
    {test_page,                "Page Structure Subsystem"     },
    {test_dma,                 "DMA Zone/Allocation Tests"    },
    {test_memory_adversarial,  "Memory Adversarial/Error Tests"},
};

static const int num_tests = sizeof(test_functions) / sizeof(test_entry_t);

/// @brief Run all kernel tests.
/// @return 0 on success, -1 on failure.
int kernel_run_tests(void)
{
    pr_notice("Starting kernel tests...\n");
    int passed = 0;
    for (int i = 0; i < num_tests; i++) {
        pr_notice("Running test %2d of %2d: %s...\n", i + 1, num_tests, test_functions[i].name);
        test_functions[i].func();
        passed++;
    }
    pr_notice("Kernel tests completed: %d/%d passed\n", passed, num_tests);

    return (passed == num_tests) ? 0 : -1;
}
