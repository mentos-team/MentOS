///                MentOS, The Mentoring Operating system project
/// @file list.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "list.h"
#include "assert.h"
#include "stdlib.h"

list_t *list_create()
{
	list_t *list = calloc(sizeof(list_t), 1);

	return list;
}

size_t list_size(list_t *list)
{
	assert(list && "List is null.");

	return list->size;
}

bool_t list_empty(list_t *list)
{
	assert(list && "List is null.");

	if (list->size == 0) {
		return true;
	}

	return false;
}

listnode_t *list_insert_front(list_t *list, void *value)
{
	assert(list && "List is null.");
	assert(value && "Value is null.");

	// Create a new node.
	listnode_t *node = calloc(sizeof(listnode_t), 1);

	list->head->prev = node;
	node->next = list->head;
	node->value = value;

	// If it's the first element, then it's both head and tail
	if (!list->head) {
		list->tail = node;
	}

	list->head = node;
	list->size++;

	return node;
}

listnode_t *list_insert_back(list_t *list, void *value)
{
	assert(list && "List is null.");
	assert(value && "Value is null.");

	// Create a new node.
	listnode_t *node = calloc(sizeof(listnode_t), 1);
	node->prev = list->tail;

	if (list->tail) {
		list->tail->next = node;
	}
	node->value = value;

	if (!list->head) {
		list->head = node;
	}

	list->tail = node;
	list->size++;

	return node;
}

void list_insert_node_back(list_t *list, listnode_t *node)
{
	assert(list && "List is null.");
	assert(node && "Node is null.");

	node->prev = list->tail;
	if (list->tail) {
		list->tail->next = node;
	}

	if (!list->head) {
		list->head = node;
	}

	list->tail = node;
	list->size++;
}

void *list_remove_node(list_t *list, listnode_t *node)
{
	assert(list && "List is null.");
	assert(node && "Node is null.");

	if (list->head == node) {
		return list_remove_front(list);
	} else if (list->tail == node) {
		return list_remove_back(list);
	}

	void *value = node->value;
	node->next->prev = node->prev;
	node->prev->next = node->next;
	list->size--;
	free(node);

	return value;
}

void *list_remove_front(list_t *list)
{
	assert(list && "List is null.");

	if (list->head == NULL) {
		return NULL;
	}

	listnode_t *node = list->head;
	void *value = node->value;
	list->head = node->next;

	if (list->head) {
		list->head->prev = NULL;
	}
	free(node);
	list->size--;

	return value;
}

void *list_remove_back(list_t *list)
{
	assert(list && "List is null.");

	if (list->head == NULL) {
		return NULL;
	}

	listnode_t *node = list->tail;
	void *value = node->value;
	list->tail = node->prev;

	if (list->tail) {
		list->tail->next = NULL;
	}
	free(node);
	list->size--;

	return value;
}

listnode_t *list_find(list_t *list, void *value)
{
	listnode_foreach(listnode, list)
	{
		if (listnode->value == value) {
			return listnode;
		}
	}

	return NULL;
}

void list_push(list_t *list, void *value)
{
	assert(list && "List is null.");
	assert(value && "Value is null.");

	list_insert_back(list, value);
}

listnode_t *list_pop_back(list_t *list)
{
	assert(list && "List is null.");

	if (!list->head) {
		return NULL;
	}

	listnode_t *node = list->tail;
	list->tail = node->prev;

	if (list->tail) {
		list->tail->next = NULL;
	}

	list->size--;

	return node;
}

listnode_t *list_pop_front(list_t *list)
{
	assert(list && "List is null.");

	if (!list->head) {
		return NULL;
	}

	listnode_t *node = list->head;
	list->head = list->head->next;
	list->size--;

	return node;
}

void list_enqueue(list_t *list, void *value)
{
	assert(list && "List is null.");
	assert(value && "Value is null.");

	list_insert_front(list, value);
}

listnode_t *list_dequeue(list_t *list)
{
	assert(list && "List is null.");

	return list_pop_back(list);
}

void *list_peek_front(list_t *list)
{
	assert(list && "List is null.");

	if (!list->head) {
		return NULL;
	}

	return list->head->value;
}

void *list_peek_back(list_t *list)
{
	assert(list && "List is null.");

	if (!list->tail) {
		return NULL;
	}

	return list->tail->value;
}

int list_contain(list_t *list, void *value)
{
	assert(list && "List is null.");
	assert(value && "Value is null.");

	int idx = 0;
	listnode_foreach(listnode, list)
	{
		if (listnode->value == value) {
			return idx;
		}
		++idx;
	}

	return -1;
}

listnode_t *list_get_node_by_index(list_t *list, size_t index)
{
	assert(list && "List is null.");

	if (index >= list_size(list)) {
		return NULL;
	}

	size_t curr = 0;
	listnode_foreach(listnode, list)
	{
		if (index == curr) {
			return listnode;
		}
		curr++;
	}
	return NULL;
}

void *list_remove_by_index(list_t *list, size_t index)
{
	assert(list && "List is null.");

	listnode_t *node = list_get_node_by_index(list, index);
	if (node != NULL) {
		return list_remove_node(list, node);
	}

	return NULL;
}

void list_destroy(list_t *list)
{
	assert(list && "List is null.");

	// Free each node's value and the node itself.
	listnode_t *node = list->head;
	while (node != NULL) {
		listnode_t *save = node;
		node = node->next;
		free(save);
	}

	// Free the list.
	free(list);
}

void listnode_destroy(listnode_t *node)
{
	assert(node && "Node is null.");

	free(node);
}

void list_merge(list_t *target, list_t *source)
{
	assert(target && "Target list is null.");
	assert(source && "Source list is null.");

	// Destructively merges source into target.
	if (target->tail) {
		target->tail->next = source->head;
	} else {
		target->head = source->head;
	}

	if (source->tail) {
		target->tail = source->tail;
	}

	target->size += source->size;
	free(source);
}
