///                MentOS, The Mentoring Operating system project
/// @file list.h
/// @brief An implementation for generic list.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"

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
	size_t size;
} list_t;

/// @brief Macro used to iterate through a list.
#define listnode_foreach(it, list)                                             \
	for (listnode_t * (it) = (list)->head; (it) != NULL; (it) = (it)->next)

/// @brief Create a list and set head, tail to NULL, and size to 0.
list_t *list_create();

/// @brief Get list size.
size_t list_size(list_t *list);

/// @brief Checks if the list is empty.
bool_t list_empty(list_t *list);

/// @brief Insert a value at the front of list.
listnode_t *list_insert_front(list_t *list, void *value);

/// @brief Insert a value at the back of list.
listnode_t *list_insert_back(list_t *list, void *value);

/// @brief Insert a value at the back of list.
void list_insert_node_back(list_t *list, listnode_t *item);

/// @brief Given a listnode, remove it from lis.
void *list_remove_node(list_t *list, listnode_t *node);

/// @brief Remove a value at the front of list.
void *list_remove_front(list_t *list);

/// @brief Remove a value at the back of list.
void *list_remove_back(list_t *list);

/// @brief Searches the node of the list which points at the given value.
listnode_t *list_find(list_t *list, void *value);

/// @brief Insert after tail of list(same as insert back).
void list_push(list_t *list, void *value);

/// @brief   Remove and return the tail of the list.
/// @details User is responsible for freeing the returned node and the value.
listnode_t *list_pop_back(list_t *list);

/// @brief   Remove and return the head of the list.
/// @details User is responsible for freeing the returned node and the value.
listnode_t *list_pop_front(list_t *list);

/// @brief Insert before head of list(same as insert front).
void list_enqueue(list_t *list, void *value);

/// @brief Remove and return tail of list(same as list_pop).
listnode_t *list_dequeue(list_t *list);

/// @brief Get the value of the first element but not remove it.
void *list_peek_front(list_t *list);

/// @brief Get the value of the last element but not remove it.
void *list_peek_back(list_t *list);

/// @brief Destory a list.
void list_destroy(list_t *list);

/// @brief Destroy a node of the list.
void listnode_destroy(listnode_t *node);

/// @brief Does the list contain a value (Return -1 if list element is not
///        found).
int list_contain(list_t *list, void *value);

/// @brief Returns the node at the given index.
listnode_t *list_get_node_by_index(list_t *list, size_t index);

/// @brief Removes a node from the list at the given index.
void *list_remove_by_index(list_t *list, size_t index);

/// @brief Append source at the end of target and DESTROY source.
void list_merge(list_t *target, list_t *source);
