/// @file list.h
/// @brief An implementation for generic list.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Represent the node of a list.
typedef struct listnode_t {
    /// A pointer to the value.
    void *value;
    /// The previous node.
    struct listnode_t *prev;
    /// The next node.
    struct listnode_t *next;
} listnode_t;

/// @brief Represent the list.
typedef struct list_t {
    /// The first element of the list.
    listnode_t *head;
    /// The last element of the list.
    listnode_t *tail;
    /// The size of the list.
    unsigned int size;
} list_t;

/// @brief Macro used to iterate through a list.
#define listnode_foreach(it, list) \
    for (listnode_t * (it) = (list)->head; (it) != NULL; (it) = (it)->next)

/// @brief Create a list and set head, tail to NULL, and size to 0.
/// @return The newly created list.
list_t *list_create();

/// @brief Get list size.
/// @param list The list.
/// @return The size of the list.
unsigned int list_size(list_t *list);

/// @brief Checks if the list is empty.
/// @param list The list.
/// @return 1 if empty, 0 otherwise.
int list_empty(list_t *list);

/// @brief Insert a value at the front of list.
/// @param list  The list.
/// @param value The value to insert.
/// @return The node associated with the inserted value.
listnode_t *list_insert_front(list_t *list, void *value);

/// @brief Insert a value at the back of list.
/// @param list  The list.
/// @param value The value to insert.
/// @return The node associated with the inserted value.
listnode_t *list_insert_back(list_t *list, void *value);

/// @brief Given a listnode, remove it from list.
/// @param list The list.
/// @param node The node that has to be removed.
/// @return The value associated with the removed node.
void *list_remove_node(list_t *list, listnode_t *node);

/// @brief Remove a value at the front of list.
/// @param list The list.
/// @return The value associated with the removed node.
void *list_remove_front(list_t *list);

/// @brief Remove a value at the back of list.
/// @param list The list.
/// @return The value associated with the removed node.
void *list_remove_back(list_t *list);

/// @brief Searches the node of the list which points at the given value.
/// @param list  The list.
/// @param value The value that has to be searched.
/// @return The node associated with the value.
listnode_t *list_find(list_t *list, void *value);

/// @brief Insert after tail of list(same as insert back).
/// @param list  The list.
/// @param value The value to insert.
void list_push_back(list_t *list, void *value);

/// @brief Remove and return the tail of the list.
/// @param list  The list.
/// @return The node that has been removed.
/// @details User is responsible for freeing the returned node and the value.
listnode_t *list_pop_back(list_t *list);

/// @brief Insert before head of list(same as insert front).
/// @param list  The list.
/// @param value The value to insert.
void list_push_front(list_t *list, void *value);

/// @brief Remove and return the head of the list.
/// @param list  The list.
/// @return The node that has been removed.
/// @details User is responsible for freeing the returned node and the value.
listnode_t *list_pop_front(list_t *list);

/// @brief Get the value of the first element but not remove it.
/// @param list The list.
/// @return The value associated with the first node.
void *list_peek_front(list_t *list);

/// @brief Get the value of the last element but not remove it.
/// @param list The list.
/// @return The value associated with the first node.
void *list_peek_back(list_t *list);

/// @brief Destroy a list.
/// @param list The list.
void list_destroy(list_t *list);

/// @brief Checks if the given value is contained inside the list.
/// @param list  The list.
/// @param value The value to search.
/// @return -1 if list element is not found, the index otherwise.
int list_get_index_of_value(list_t *list, void *value);

/// @brief Returns the node at the given index.
/// @param list  The list.
/// @param index The index of the desired node.
/// @return A pointer to the node, or NULL otherwise.
listnode_t *list_get_node_by_index(list_t *list, unsigned int index);

/// @brief Removes a node from the list at the given index.
/// @param list  The list.
/// @param index The index of the node we need to remove.
/// @return The value contained inside the node, NULL otherwise.
void *list_remove_by_index(list_t *list, unsigned int index);

/// @brief Append source at the end of target.
/// @param target Where the element are added.
/// @param source Where the element are removed.
/// @details Beware, source is destroyed.
void list_merge(list_t *target, list_t *source);
