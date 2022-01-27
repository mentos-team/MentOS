/// @file list.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "klib/list.h"
#include "assert.h"
#include "string.h"
#include "mem/slab.h"

static inline listnode_t *__node_alloc()
{
    listnode_t *node = kmalloc(sizeof(listnode_t));
    memset(node, 0, sizeof(listnode_t));
    return node;
}

static inline void __node_dealloc(listnode_t *node)
{
    assert(node && "Invalid pointer to node.");
    kfree(node);
}

static inline void __list_dealloc(list_t *list)
{
    assert(list && "Invalid pointer to list.");
    kfree(list);
}

list_t *list_create()
{
    list_t *list = kmalloc(sizeof(list_t));
    memset(list, 0, sizeof(list_t));
    return list;
}

unsigned int list_size(list_t *list)
{
    assert(list && "List is null.");

    return list->size;
}

int list_empty(list_t *list)
{
    assert(list && "List is null.");

    if (list->size == 0) {
        return 1;
    }

    return 0;
}

listnode_t *list_insert_front(list_t *list, void *value)
{
    assert(list && "List is null.");
    assert(value && "Value is null.");

    // Create a new node.
    listnode_t *node = __node_alloc();

    list->head->prev = node;
    node->next       = list->head;
    node->value      = value;

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
    listnode_t *node = __node_alloc();
    node->prev       = list->tail;

    if (list->tail != NULL) {
        list->tail->next = node;
    }
    node->value = value;

    if (list->head == NULL) {
        list->head = node;
    }

    list->tail = node;
    list->size++;
    return node;
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

    void *value      = node->value;
    node->next->prev = node->prev;
    node->prev->next = node->next;
    list->size--;
    __node_dealloc(node);

    return value;
}

void *list_remove_front(list_t *list)
{
    assert(list && "List is null.");

    if (list->head == NULL) {
        return NULL;
    }

    listnode_t *node = list->head;
    void *value      = node->value;
    list->head       = node->next;

    if (list->head) {
        list->head->prev = NULL;
    }
    __node_dealloc(node);
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
    void *value      = node->value;
    list->tail       = node->prev;

    if (list->tail) {
        list->tail->next = NULL;
    }
    __node_dealloc(node);
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

void list_push_back(list_t *list, void *value)
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
    list->tail       = node->prev;

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
    list->head       = list->head->next;
    list->size--;

    return node;
}

void list_push_front(list_t *list, void *value)
{
    assert(list && "List is null.");
    assert(value && "Value is null.");

    list_insert_front(list, value);
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

int list_get_index_of_value(list_t *list, void *value)
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

listnode_t *list_get_node_by_index(list_t *list, unsigned int index)
{
    assert(list && "List is null.");

    if (index >= list_size(list)) {
        return NULL;
    }

    unsigned int curr = 0;
    listnode_foreach(listnode, list)
    {
        if (index == curr) {
            return listnode;
        }
        curr++;
    }
    return NULL;
}

void *list_remove_by_index(list_t *list, unsigned int index)
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

    // Deallocate each node.
    listnode_t *node = list->head;
    while (node != NULL) {
        listnode_t *save = node;
        node             = node->next;
        __node_dealloc(save);
    }

    // Free the list.
    __list_dealloc(list);
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
    __list_dealloc(source);
}
