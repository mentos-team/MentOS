/// @file list.c
/// @brief Source file for list operations, implementing list manipulation functions.

#include "list.h"

void list_init(list_t *list, listnode_t *(*alloc_fn)(void), void (*dealloc_fn)(listnode_t *))
{
    assert(list && "List is null.");
    list_head_init(&list->head);
    list->size    = 0;
    list->alloc   = alloc_fn;
    list->dealloc = dealloc_fn;
}

listnode_t *list_insert_front(list_t *list, void *value)
{
    assert(list && "List is null.");
    assert(value && "Value is null.");
    listnode_t *node = list->alloc();
    assert(node && "Failed to allocate node.");
    node->value = value;
    list_head_insert_after(&node->list, &list->head);
    list->size++;
    return node;
}

listnode_t *list_insert_back(list_t *list, void *value)
{
    assert(list && "List is null.");
    assert(value && "Value is null.");
    listnode_t *node = list->alloc();
    assert(node && "Failed to allocate node.");
    node->value = value;
    list_head_insert_before(&node->list, &list->head);
    list->size++;
    return node;
}

void *list_remove_node(list_t *list, listnode_t *node)
{
    assert(list && "List is null.");
    assert(node && "Node is null.");
    void *value = node->value;
    list_head_remove(&node->list);
    list->dealloc(node);
    list->size--;
    return value;
}

void *list_remove_front(list_t *list)
{
    assert(list && "List is null.");
    if (list_head_empty(&list->head)) {
        return NULL;
    }
    listnode_t *node = list_entry(list->head.next, listnode_t, list);
    return list_remove_node(list, node);
}

void *list_remove_back(list_t *list)
{
    assert(list && "List is null.");
    if (list_head_empty(&list->head)) {
        return NULL;
    }
    listnode_t *node = list_entry(list->head.prev, listnode_t, list);
    return list_remove_node(list, node);
}

void list_destroy(list_t *list)
{
    assert(list && "List is null.");
    list_for_each_safe_decl(entry, store, &list->head)
    {
        listnode_t *it = list_entry(entry, listnode_t, list);
        list->dealloc(it);
    }
    list_head_init(&list->head);
    list->size = 0;
}

listnode_t *list_find(list_t *list, void *value)
{
    assert(list && "List is null.");
    assert(value && "Value is null.");
    list_for_each_safe_decl(entry, store, &list->head)
    {
        listnode_t *it = list_entry(entry, listnode_t, list);
        if (it->value == value) {
            return it;
        }
    }
    return NULL;
}

void *list_peek_front(const list_t *list)
{
    assert(list && "List is null.");
    if (list_empty(list)) {
        return NULL;
    }
    listnode_t *front_node = list_entry(list->head.next, listnode_t, list);
    return front_node->value;
}

void *list_peek_back(const list_t *list)
{
    assert(list && "List is null.");
    if (list_empty(list)) {
        return NULL;
    }
    listnode_t *back_node = list_entry(list->head.prev, listnode_t, list);
    return back_node->value;
}

void list_merge(list_t *target, list_t *source)
{
    assert(target && "Target list is null.");
    assert(source && "Source list is null.");
    if (!list_empty(source)) {
        listnode_t *target_last  = list_entry(target->head.prev, listnode_t, list);
        listnode_t *source_first = list_entry(source->head.next, listnode_t, list);

        target_last->list.next  = &source_first->list;
        source_first->list.prev = &target_last->list;

        listnode_t *source_last = list_entry(source->head.prev, listnode_t, list);
        source_last->list.next  = &target->head;
        target->head.prev       = &source_last->list;

        target->size += source->size;
        list_head_init(&source->head);
        source->size = 0;
    }
}
