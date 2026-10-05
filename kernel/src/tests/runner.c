/// @file runner.c
/// @brief Kernel test runner.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
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
extern void test_fpu(void);
extern void test_vfs(void);
extern void test_video(void);

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
    {test_vfs,                 "VFS Subsystem"                },
    {test_dma,                 "DMA Zone/Allocation Tests"    },
    {test_memory_adversarial,  "Memory Adversarial/Error Tests"},
    {test_fpu,                 "FPU Subsystem"                },
    {test_video,               "Console/Video Subsystem"      },
};

static const int num_tests = sizeof(test_functions) / sizeof(test_entry_t);

/// @brief Number of assertions that failed so far.
static unsigned failed_assertions = 0;

void kernel_test_record_failure(void) { ++failed_assertions; }

unsigned kernel_test_failures(void) { return failed_assertions; }

/// @brief Run all kernel tests.
/// @details Every suite runs even if an earlier one failed. A suite passes if
/// none of its assertions failed.
/// @return 0 if every suite passed, -1 otherwise.
int kernel_run_tests(void)
{
    pr_notice("Starting kernel tests...\n");
    int passed = 0;
    for (int i = 0; i < num_tests; i++) {
        pr_notice("Running test %2d of %2d: %s...\n", i + 1, num_tests, test_functions[i].name);
        unsigned before = failed_assertions;
        test_functions[i].func();
        if (failed_assertions == before) {
            passed++;
        } else {
            pr_emerg("Suite '%s' FAILED: %u failed assertion(s)\n", test_functions[i].name, failed_assertions - before);
        }
    }
    pr_notice("Kernel tests completed: %d/%d suites passed, %u failed assertion(s)\n", passed, num_tests, failed_assertions);

    return (passed == num_tests) ? 0 : -1;
}
