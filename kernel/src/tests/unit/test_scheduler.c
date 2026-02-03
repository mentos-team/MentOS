/// @file test_scheduler.c
/// @brief Scheduler subsystem unit tests - Non-destructive version.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "process/scheduler.h"
#include "tests/test.h"
#include "tests/test_utils.h"

/// @brief Test scheduler runqueue structure size.
TEST(scheduler_runqueue_structure)
{
    TEST_SECTION_START("Scheduler runqueue structure");

    ASSERT(sizeof(runqueue_t) > 0);
    ASSERT(sizeof(sched_param_t) > 0);

    TEST_SECTION_END();
}

/// @brief Test scheduler constants.
TEST(scheduler_constants)
{
    TEST_SECTION_START("Scheduler constants");

    ASSERT(MAX_PROCESSES == 256);

    TEST_SECTION_END();
}

/// @brief Test current process is accessible.
TEST(scheduler_current_process)
{
    TEST_SECTION_START("Scheduler current process");

    task_struct *current = scheduler_get_current_process();
    ASSERT_MSG(current != NULL, "Current process must be accessible");

    TEST_SECTION_END();
}

/// @brief Test active process count is reasonable.
TEST(scheduler_active_processes)
{
    TEST_SECTION_START("Scheduler active processes");

    size_t active = scheduler_get_active_processes();
    ASSERT_MSG(active > 0, "Must have at least one active process");
    ASSERT_MSG(active <= MAX_PROCESSES, "Active processes must not exceed max");

    TEST_SECTION_END();
}

/// @brief Test init process exists.
TEST(scheduler_init_process)
{
    TEST_SECTION_START("Scheduler init process");

    extern task_struct *init_process;
    ASSERT_MSG(init_process != NULL, "Init process must be initialized");
    ASSERT_MSG(init_process->pid == 1, "Init process PID must be 1");

    TEST_SECTION_END();
}

/// @brief Test current process has valid PID.
TEST(scheduler_current_pid_valid)
{
    TEST_SECTION_START("Scheduler current PID valid");

    task_struct *current = scheduler_get_current_process();
    ASSERT_MSG(current != NULL, "Current process must exist");
    ASSERT_MSG(current->pid > 0, "Current process PID must be positive");
    ASSERT_MSG(current->pid < MAX_PROCESSES, "Current process PID must be within range");

    TEST_SECTION_END();
}

/// @brief Test scheduler can find running process by PID.
TEST(scheduler_find_running_process)
{
    TEST_SECTION_START("Scheduler find running process");

    task_struct *current = scheduler_get_current_process();
    ASSERT_MSG(current != NULL, "Current process must exist");

    task_struct *found = scheduler_get_running_process(current->pid);
    ASSERT_MSG(found != NULL, "Should be able to find current process");
    ASSERT_MSG(found->pid == current->pid, "Found process PID should match");

    TEST_SECTION_END();
}

/// @brief Test scheduler vruntime is reasonable.
TEST(scheduler_vruntime)
{
    TEST_SECTION_START("Scheduler vruntime");

    time_t max_vruntime = scheduler_get_maximum_vruntime();
    ASSERT_MSG(max_vruntime >= 0, "Maximum vruntime must be non-negative");

    TEST_SECTION_END();
}

/// @brief Main test function for scheduler subsystem.
/// This function runs all scheduler tests in sequence.
void test_scheduler(void)
{
    test_scheduler_runqueue_structure();
    test_scheduler_constants();
    test_scheduler_current_process();
    test_scheduler_active_processes();
    test_scheduler_init_process();
    test_scheduler_current_pid_valid();
    test_scheduler_find_running_process();
    test_scheduler_vruntime();
}
