/// @file list_head_algorithm.h
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief Some general algorithm that might come in handy while using list_head.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "list_head.h"

/// @brief list_head comparison function.
typedef int (*list_head_compare)(const list_head *, const list_head *);

/// @brief -
/// @param list
/// @param compare
static inline void list_head_sort(list_head *list, list_head_compare compare)
{
    list_head *current = NULL, *index = NULL, *next = NULL;
    // Check whether list is empty
    if (!list_head_empty(list)) {
        // Keeps track if we need to restart the outer loop.
        int restart = 0;
        // Current will point to head
        for (current = list->next; current->next != list;) {
            // Save pointer to next.
            next = current->next;
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
