/// @file list_head.h
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// @brief Structure used to implement the list_head data structure.
typedef struct list_head {
    /// @brief The previous element.
    struct list_head *prev;
    /// @brief The subsequent element.
    struct list_head *next;
} list_head;

/// @brief Get the struct for this entry.
/// @param ptr    The &list_head pointer.
/// @param type   The type of the struct this is embedded in.
/// @param member The name of the list_head within the struct.
#define list_entry(ptr, type, member) container_of(ptr, type, member)

/// @brief Iterates over a list.
/// @param pos the name of the iterator used to visit the list.
/// @param head the head for your list.
#define list_for_each(pos, head) \
    for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

/// @brief Iterates over a list safe against removal of list entry.
/// @param pos the name of the iterator used to visit the list.
/// @param store another list iterator to use as temporary storage.
/// @param head the head for your list.
#define list_for_each_safe(pos, store, head)                           \
    for ((pos) = (head)->next, (store) = (pos)->next; (pos) != (head); \
         (pos) = (store), (store) = (pos)->next)

/// @brief Iterates over a list, but declares the iterator.
/// @param pos the name of the iterator used to visit the list.
/// @param head the head for your list.
#define list_for_each_decl(pos, head) \
    for (list_head * (pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

/// @brief Iterates over a list backwards.
/// @param pos the name of the iterator used to visit the list.
/// @param head the head for your list.
#define list_for_each_prev(pos, head) \
    for ((pos) = (head)->prev; (pos) != (head); (pos) = (pos)->prev)

/// @brief Iterates over a list backwards, but declares the iterator.
/// @param pos the name of the iterator used to visit the list.
/// @param head the head for your list.
#define list_for_each_prev_decl(pos, head) \
    for (list_head * (pos) = (head)->prev; (pos) != (head); (pos) = (pos)->prev)

/// @brief Initializes the list_head.
/// @param head The head of your list.
static inline void list_head_init(list_head *head)
{
    head->next = head->prev = head;
}

/// @brief Tests whether the given list is empty.
/// @param head The list to check.
/// @return 1 if empty, 0 otherwise.
static inline int list_head_empty(const list_head *head)
{
    return head->next == head;
}

/// @brief Computes the size of the list.
/// @param head The head of the list.
/// @return the size of the list.
static inline unsigned list_head_size(const list_head *head)
{
    unsigned size = 0;
    if (!list_head_empty(head))
        list_for_each_decl(it, head) size += 1;
    return size;
}

/// @brief Insert the new entry after the given location.
/// @param new_entry the new element we want to insert.
/// @param location the element after which we insert.
static inline void list_head_insert_after(list_head *new_entry, list_head *location)
{
    // We store the old `next` element.
    list_head *old_next = location->next;
    // We insert our element.
    location->next = new_entry;
    // We update the `previous` link of our new entry.
    new_entry->prev = location;
    // We update the `next` link of our new entry.
    new_entry->next = old_next;
    // We link the previously `next` element to our new entry.
    old_next->prev = new_entry;
}

/// @brief Insert the new entry before the given location.
/// @param new_entry the new element we want to insert.
/// @param location the element after which we insert.
static inline void list_head_insert_before(list_head *new_entry, list_head *location)
{
    // We store the old `previous` element.
    list_head *old_prev = location->prev;
    // We link the old `previous` element to our new entry.
    old_prev->next = new_entry;
    // We update the `previous` link of our new entry.
    new_entry->prev = old_prev;
    // We update the `next` link of our new entry.
    new_entry->next = location;
    // Finally, we close the link with the old insertion location element.
    location->prev = new_entry;
}

/// @brief Removes the given entry from the list it is contained in.
/// @param entry the entry we want to remove.
static inline void list_head_remove(list_head *entry)
{
    // Check if the element is actually in a list.
    if (!list_head_empty(entry)) {
        // We link the `previous` element to the `next` one.
        entry->prev->next = entry->next;
        // We link the `next` element to the `previous` one.
        entry->next->prev = entry->prev;
        // We initialize the entry again.
        list_head_init(entry);
    }
}

/// @brief Removes an element from the list, it's used when we have a possibly
/// null list pointer and want to pop an element from it.
/// @param head the head of the list.
/// @return a list_head pointing to the element we removed, NULL on failure.
static inline list_head *list_head_pop(list_head *head)
{
    // Check if the list is not empty.
    if (!list_head_empty(head)) {
        // Store the pointer.
        list_head *value = head->next;
        // Remove the element from the list.
        list_head_remove(head->next);
        // Return the pointer to the element.
        return value;
    }
    return NULL;
}

/// @brief Append the `secondary` list at the end of the `main` list.
/// @param main the main list where we append the secondary list.
/// @param secondary the secondary list, which gets appended, and re-initialized as empty.
static inline void list_head_append(list_head *main, list_head *secondary)
{
    // Check that both lists are actually filled with entries.
    if (!list_head_empty(main) && !list_head_empty(secondary)) {
        // Connect the last element of the main list to the first one of the secondary list.
        main->prev->next = secondary->next;
        // Connect the first element of the secondary list to the last one of the main list.
        secondary->next->prev = main->prev;

        // Connect the last element of the secondary list to our main.
        secondary->prev->next = main;
        // Connect our main to the last element of the secondary list.
        main->prev = secondary->prev;

        // Re-initialize the secondary list.
        list_head_init(secondary);
    }
}

/// @brief Replaces entry1 with entry2, entry1 will be removed from the list.
/// @param entry1 the first entry to remove.
/// @param entry2 the second entry which will take the place of the first entry.
static inline void list_head_replace(list_head *entry1, list_head *entry2)
{
    // First we need to remove the second entry.
    list_head_remove(entry2);
    // Then, we can place second entry where the first entry is.
    entry2->next       = entry1->next;
    entry2->next->prev = entry2;
    entry2->prev       = entry1->prev;
    entry2->prev->next = entry2;
    // Re-initialize the first entry.
    list_head_init(entry1);
}

/// @brief Swaps entry1 and entry2 inside the list.
/// @param entry1 the first entry.
/// @param entry2 the second entry.
static inline void list_head_swap(list_head *entry1, list_head *entry2)
{
    list_head *pos = entry2->prev;
    list_head_replace(entry1, entry2);
    if (pos == entry1)
        pos = entry2;
    list_head_insert_after(entry1, pos);
}
