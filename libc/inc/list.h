/// @file list.h
/// @brief An implementation for a generic list.
/// @details Provides a generic doubly linked list with allocation support.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "list_head.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

/// @brief Represents the node of a list.
typedef struct listnode_t {
    list_head list; ///< List structure for this node.
    void *value;    ///< Pointer to node's value.
} listnode_t;

/// @brief Represents the list.
typedef struct list_t {
    list_head head;                ///< Head of the list.
    unsigned int size;             ///< Size of the list.
    listnode_t *(*alloc)(void);    ///< Node allocation function.
    void (*dealloc)(listnode_t *); ///< Node deallocation function.
} list_t;

/// @brief Initializes the list with custom alloc and dealloc functions.
/// @param list The list to initialize.
/// @param alloc_fn Function to allocate nodes.
/// @param dealloc_fn Function to deallocate nodes.
void list_init(list_t *list, listnode_t *(*alloc_fn)(void), void (*dealloc_fn)(listnode_t *));

/// @brief Returns the size of the list.
/// @param list The list to get the size of.
/// @return The number of elements in the list.
static inline unsigned int list_size(const list_t *list)
{
    assert(list && "List is null.");
    return list->size;
}

/// @brief Checks if the list is empty.
/// @param list The list to check.
/// @return 1 if the list is empty, 0 otherwise.
static inline int list_empty(const list_t *list)
{
    assert(list && "List is null.");
    return list_head_empty(&list->head);
}

/// @brief Insert a new element at the front of the list.
/// @param list The list to insert into.
/// @param value The value to store in the new node.
/// @return Pointer to the new node.
listnode_t *list_insert_front(list_t *list, void *value);

/// @brief Insert a new element at the back of the list.
/// @param list The list to insert into.
/// @param value The value to store in the new node.
/// @return Pointer to the new node.
listnode_t *list_insert_back(list_t *list, void *value);

/// @brief Removes a specific node from the list.
/// @param list The list to remove the node from.
/// @param node The node to remove.
/// @return The value stored in the removed node.
void *list_remove_node(list_t *list, listnode_t *node);

/// @brief Removes the front element of the list.
/// @param list The list to remove from.
/// @return The value of the removed front node.
void *list_remove_front(list_t *list);

/// @brief Removes the back element of the list.
/// @param list The list to remove from.
/// @return The value of the removed back node.
void *list_remove_back(list_t *list);

/// @brief Destroys the list and deallocates all nodes.
/// @param list The list to destroy.
void list_destroy(list_t *list);

/// @brief Finds the node with the specified value.
/// @param list The list to search.
/// @param value The value to search for.
/// @return Pointer to the node if found, NULL otherwise.
listnode_t *list_find(list_t *list, void *value);

/// @brief Returns the value at the front of the list without removing it.
/// @param list The list to peek at.
/// @return The value at the front of the list, or NULL if the list is empty.
void *list_peek_front(const list_t *list);

/// @brief Returns the value at the back of the list without removing it.
/// @param list The list to peek at.
/// @return The value at the back of the list, or NULL if the list is empty.
void *list_peek_back(const list_t *list);

/// @brief Merges two lists destructively, appending the source list to the target.
/// @param target The target list to append to.
/// @param source The source list to merge from, which will be re-initialized as empty.
void list_merge(list_t *target, list_t *source);
