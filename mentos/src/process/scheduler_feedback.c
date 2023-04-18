/// @file scheduler_feedback.c
/// @brief Manage the current PID for the scheduler feedback session
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[SCHFBK]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_DEBUG

/*
HOW TO:
all'avvio di MentOS, usa comando start, verrai avvisato su terminale quando la sessione sarà terminata.
Puoi verificare i risultati a video lanciando il comando cat sul file del desktop (feedback.txt)

es:
~  start
~  cat feedback.txt

Questa e' una versione di prova in cui registriamo una brevissima sessione di soli 10 pid.
(Funzionante per ora solo con lo scheduler RR)
Istruzioni relative ai parametri della funzione start (che sono work in progress) sono in start.c
*/

#include "process/scheduler_feedback.h"
#include "hardware/timer.h"
#include "strerror.h"
#include "io/debug.h"
#include "fs/vfs.h"
#include "assert.h"
#include "string.h"
#include "fcntl.h"
#include "stdio.h"

/// @brief
#define FEEDBACK_FILENAME "/var/feedback.txt"
/// @brief
#define FEEDBACK_HEADER "\nPID PRIO NAME\n\0"
/// @brief
#define LOG_INTERVAL 3

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

/// @brief
ssize_t offset;
/// @brief
size_t next_log;
/// @brief
size_t total_occurrences;

/// @brief
struct statistic {
    pid_t pid;
    char name[20];
    unsigned long occur;
} arr_stats[PID_MAX_LIMIT];

int scheduler_feedback_init()
{
    // Create the feedback file, if necessary.
    int ret = vfs_mkdir("/var", 0644);
    if ((ret < 0) && (-ret != EEXIST)) {
        pr_err("Failed to create the `/var` directory.\n");
        pr_err("Error: %s\n", strerror(errno));
        return 0;
    }
    // Create the feedback file, if necessary.
    vfs_file_t *feedback = vfs_open(FEEDBACK_FILENAME, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (feedback == NULL) {
        pr_err("Failed to create the feedback file.\n");
        pr_err("Error: %s\n", strerror(errno));
        return 0;
    }
    // Get the length of the headr.
    ssize_t header_len = strlen(FEEDBACK_HEADER);
    // Reset the offset.
    offset = 0;
    // Write the header.
    vfs_write(feedback, FEEDBACK_HEADER, offset, header_len);
    // Move the offset.
    offset += header_len;
    // Close the file.
    vfs_close(feedback);
    // Initialize the stat array.
    for (size_t i = 0; i < PID_MAX_LIMIT; ++i) {
        arr_stats[i].pid   = -1;
        arr_stats[i].occur = 0;
    }
    // Set when the first logging should happen.
    next_log = timer_get_seconds() + LOG_INTERVAL;
    // Initialize the number of occurrences.
    total_occurrences = 0;
    return 1;
}

void scheduler_feedback_update(task_struct *next)
{
    assert(next && "Received a NULL task.");

    // ========================================================================

    if (next_log < timer_get_seconds()) {
        pr_debug("Scheduling Statistics (%s)\n", POLICY_NAME);

        // Open the feedback file.
        //vfs_file_t *feedback = vfs_open(FEEDBACK_FILENAME, O_WRONLY, 0644);
        //if (feedback == NULL) {
        //    pr_err("Failed to create the feedback file.\n");
        //    pr_err("Error: %s\n", strerror(errno));
        //    return;
        //}

        //char buffer[NAME_MAX * 2];
        //int written = 0;

        for (size_t i = 0; i < PID_MAX_LIMIT; ++i) {
            if (arr_stats[i].pid == -1) {
                break;
            }
            float tcpu = ((float)arr_stats[i].occur * 100.0) / total_occurrences;
            pr_debug("[%3d] %-12s -> TCPU: %.2f%% \n",
                     arr_stats[i].pid,
                     arr_stats[i].name,
                     tcpu);

            //written = sprintf(buffer, "[%3d](%s)[%f], ", arr_stats[i].pid, arr_stats[i].name, tcpu);
            //vfs_write(feedback, buffer, offset, written);
            //offset += written;
        }
        //vfs_write(feedback, "/n", offset, 1);
        //offset++;

        for (size_t i = 0; i < PID_MAX_LIMIT; ++i) {
            if (arr_stats[i].pid == -1) {
                break;
            }
            arr_stats[i].pid   = -1;
            arr_stats[i].occur = 0;
        }
        // Update when in the future, the logging should happen.
        next_log = timer_get_seconds() + LOG_INTERVAL;
        // Reset the number of occurrences.
        total_occurrences = 0;
    } else {
        for (size_t i = 0; i < PID_MAX_LIMIT; ++i) {
            // If the process we are logging is already in the stat array, increase the occurrences.
            if (arr_stats[i].pid == next->pid) {
                arr_stats[i].occur += 1;
                total_occurrences += 1;
                break;
            }

            // If we reached the unused stat entries, it means the current
            // process is not stored and we need to insert it.
            if (arr_stats[i].pid == -1) {
                arr_stats[i].pid = next->pid;
                strcpy(arr_stats[i].name, next->name);
                arr_stats[i].occur = 1;
                total_occurrences += 1;
                break;
            }
        }
    }
}
