/// @file pid_manager.c
/// @brief Manages the PIDs in the system.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "process/pid_manager.h"
#include "process/scheduler.h"
#include "stdint.h"
#include "string.h"

/// Defines the size of the bitmap that keeps track of used PIDs.
#define BITMAP_SIZE (MAX_PROCESSES / 32)

/// Bitmap for tracking used PIDs.
static uint32_t pid_bitmap[BITMAP_SIZE] = {0};

/// Keeps track of the last allocated PID.
static pid_t last_pid = 1;

void pid_manager_init(void) { memset(pid_bitmap, 0, sizeof(pid_bitmap)); }

void pid_manager_mark_used(pid_t pid) { pid_bitmap[pid / 32] |= (1 << (pid % 32)); }

void pid_manager_mark_free(pid_t pid) { pid_bitmap[pid / 32] &= ~(1 << (pid % 32)); }

pid_t pid_manager_get_free_pid(void)
{
    // Start searching from the next PID after the last allocated one.
    for (size_t offset = 0; offset < BITMAP_SIZE * 32; offset++) {
        // Calculate the current PID index with wrap-around.
        pid_t current_pid = (last_pid + offset) % (BITMAP_SIZE * 32);

        // Ensure PID 0 is skipped.
        if (current_pid == 0) {
            continue;
        }

        // Check the bitmap for availability.
        size_t i = current_pid / 32; // Index in the bitmap array.
        size_t j = current_pid % 32; // Bit position in the bitmap element.

        if ((pid_bitmap[i] & (1 << j)) == 0) {
            // Mark the PID as used.
            pid_manager_mark_used(current_pid);

            // Update the last allocated PID.
            last_pid = current_pid + 1;

            // Return the allocated PID.
            return current_pid;
        }
    }
    // No free PID found.
    return -1;
}
