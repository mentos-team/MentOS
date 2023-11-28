/// @file scheduler_feedback.c
/// @brief Manage the current PID for the scheduler feedback session
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "process/scheduler_feedback.h"
#include "assert.h"
#include "fcntl.h"
#include "fs/vfs.h"
#include "hardware/timer.h"
#include "stdio.h"
#include "strerror.h"
#include "string.h"

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[SCHFBK]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_INFO
#include "io/debug.h"

/// @brief How often the feedback is shown.
#define LOG_INTERVAL_SEC 0.5

/// @brief The name of the scheduling policy.
#if defined(SCHEDULER_RR)
#define POLICY_NAME "RR   "
#elif defined(SCHEDULER_PRIORITY)
#define POLICY_NAME "PRIO "
#elif defined(SCHEDULER_CFS)
#define POLICY_NAME "CFS  "
#elif defined(SCHEDULER_EDF)
#define POLICY_NAME "EDF  "
#elif defined(SCHEDULER_RM)
#define POLICY_NAME "RM   "
#elif defined(SCHEDULER_AEDF)
#define POLICY_NAME "AEDF "
#else
#error "You should enable a scheduling algorithm!"
#endif

/// @brief If uncommented, it writes the logging on file.
// #define WRITE_ON_FILE

#ifdef WRITE_ON_FILE
/// @brief Name of the file where the feedback statistics are saved.
#define FEEDBACK_FILENAME "/var/schedfb"
/// @brief
ssize_t offset;
/// @brief
vfs_file_t *feedback = NULL;
#endif

/// @brief When the next log should be displayed/saved, in CPU ticks.
unsigned long next_log;
/// @brief The total number of context-switches since the starting of the log
/// session.
size_t total_occurrences;

/// @brief A structure that keeps track of scheduling statistics.
struct statistic {
    task_struct *task;
    unsigned long occur;
} arr_stats[PID_MAX_LIMIT];

/// @brief Updates when the logging should happen.
static inline void __scheduler_feedback_deadline_advance(void)
{
    next_log = timer_get_ticks() + (LOG_INTERVAL_SEC * TICKS_PER_SECOND);
}

/// @brief Checks if the deadline is passed.
/// @return 1 if the deadline is passed, 0 otherwise.
static inline int __scheduler_feedback_deadline_check(void)
{
    return (next_log < timer_get_ticks());
}

/// @brief Logs the scheduling statistics either on file or on the terminal.
static inline void __scheduler_feedback_log(void)
{
    pr_info("Scheduling Statistics (%s)\n", POLICY_NAME);
#ifdef WRITE_ON_FILE
    // Open the feedback file.
    if (feedback == NULL) {
        pr_err("Failed to create the feedback file.\n");
        pr_err("Error: %s\n", strerror(errno));
        return;
    }
    char buffer[BUFSIZ];
    int written = 0;
#endif
#ifdef WRITE_ON_FILE
    written = sprintf(buffer, "TIME : %ds\n", timer_get_seconds());
    vfs_write(feedback, buffer, offset, written);
    offset += written;
#endif
    for (size_t i = 0; i < PID_MAX_LIMIT; ++i) {
        if (arr_stats[i].task) {
            float tcpu = ((float)arr_stats[i].occur * 100.0) / total_occurrences;
            pr_info("[%3d] | %-18s | -> TCPU: %.2f%% \n",
                    arr_stats[i].task->pid,
                    arr_stats[i].task->name,
                    tcpu);
#ifdef WRITE_ON_FILE
            written = sprintf(buffer, "[%3d] | %-18s | -> TCPU: %.2f%% \n",
                              arr_stats[i].task->pid,
                              arr_stats[i].task->name,
                              tcpu);
            vfs_write(feedback, buffer, offset, written);
            offset += written;
#endif
        }
    }
#ifdef WRITE_ON_FILE
    vfs_write(feedback, "\n", offset, 1);
    offset++;
#endif
}

int scheduler_feedback_init(void)
{
#ifdef WRITE_ON_FILE
    // Create the feedback file, if necessary.
    int ret = vfs_mkdir("/var", 0644);
    if ((ret < 0) && (-ret != EEXIST)) {
        pr_err("Failed to create the `/var` directory.\n");
        pr_err("Error: %s\n", strerror(errno));
        return 0;
    }
    // First close the file.
    if (feedback != NULL) {
        vfs_close(feedback);
    }
    // Create the feedback file, if necessary.
    feedback = vfs_open(FEEDBACK_FILENAME, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (feedback == NULL) {
        pr_err("Failed to create the feedback file.\n");
        pr_err("Error: %s\n", strerror(errno));
        return 0;
    }
    // Reset the offset.
    offset = 0;
#endif
    // Initialize the stat array.
    for (size_t i = 0; i < PID_MAX_LIMIT; ++i) {
        arr_stats[i].task  = NULL;
        arr_stats[i].occur = 0;
    }
    // Update when in the future, the logging should happen.
    __scheduler_feedback_deadline_advance();
    // Initialize the number of occurrences.
    total_occurrences = 0;
    return 1;
}

void scheduler_feedback_task_add(task_struct *task)
{
    assert(task && "Received a NULL task.");
    arr_stats[task->pid].occur = 1;
    arr_stats[task->pid].task  = task;
}

void scheduler_feedback_task_remove(pid_t pid)
{
    assert(pid < PID_MAX_LIMIT && "We received a wrong pid.");
    total_occurrences -= arr_stats[pid].occur;
    arr_stats[pid].occur = 0;
    arr_stats[pid].task  = NULL;
}

void scheduler_feedback_task_update(task_struct *task)
{
    assert(task && "Received a NULL task.");
    arr_stats[task->pid].occur += 1;
    total_occurrences += 1;
}

void scheduler_feedback_update(void)
{
    // If it is not yet time for the next reset, skip.
    if (!__scheduler_feedback_deadline_check()) {
        return;
    }
    // Dump on the feedback before reset.
    __scheduler_feedback_log();
    // Reset the occurences.
    for (size_t i = 0; i < PID_MAX_LIMIT; ++i) {
        if (arr_stats[i].task) {
            arr_stats[i].occur = 0;
        }
    }
    // Update when in the future, the logging should happen.
    __scheduler_feedback_deadline_advance();
    // Reset the number of occurrences.
    total_occurrences = 0;
}
