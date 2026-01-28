/// @file list_head_algorithm.h
/// @brief Some general algorithm that might come in handy while using list_head.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "list_head.h"

/// @brief list_head_t comparison function.
typedef int (*list_head_compare)(const list_head_t *, const list_head_t *);

/// @brief -
/// @param list
/// @param compare
static inline void list_head_sort(list_head_t *list, list_head_compare compare)
{
    list_head_t *current = NULL;
    list_head_t *index   = NULL;
    list_head_t *next    = NULL;
    // Check whether list is empty
    if (!list_head_empty(list)) {
        // Keeps track if we need to restart the outer loop.
        int restart = 0;
        // Current will point to head
        for (current = list->next; current->next != list;) {
            // Save pointer to next.
            next    = current->next;
            // Reset restart flag.
            restart = 0;
            // Index will point to node next to current
            for (index = current->next; index != list; index = index->next) {
                // If current's data is greater than index's data, swap the data of
                // current and index
                if (compare(current, index)) {
                    list_head_swap(index, current);
                    restart = 1;
                }
            }
            // Check if we need to restart.
            if (restart) {
                current = list->next;
            } else {
                current = next;
            }
        }
    }
}
