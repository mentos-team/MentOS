///                MentOS, The Mentoring Operating system project
/// @file   list_head.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// @brief Structure used to implement the list_head data structure.
typedef struct list_head
{
    /// @brief The previous element.
    struct list_head * prev;
    /// @brief The subsequent element.
    struct list_head * next;
} list_head;

/// @brief Get the struct for this entry.
/// @param ptr    The &struct list_head pointer.
/// @param type   The type of the struct this is embedded in.
/// @param member The name of the list_head within the struct.
#define list_entry(ptr, type, member) \
    ((type *) ((char *) (ptr) - (unsigned long) (&((type *) 0)->member)))

/// @brief Iterates over a list.
/// @param pos    The &struct list_head to use as a loop cursor.
/// @param head   The head for your list.
#define list_for_each(pos, head) \
    for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

/// @brief Iterates over a list backwards.
/// @param pos    The &struct list_head to use as a loop cursor.
/// @param head   The head for your list.
#define list_for_each_prev(pos, head) \
    for ((pos) = (head)->prev; (pos) != (head); (pos) = (pos)->prev)

/// @brief Iterates over a list safe against removal of list entry.
/// @param pos    The &struct list_head to use as a loop cursor.
/// @param store  Another &struct list_head to use as temporary storage.
/// @param head   The head for your list.
#define list_for_each_safe(pos, store, head) \
    for ((pos) = (head)->next, (store) = (pos)->next; (pos) != (head); \
        (pos) = (store), (store) = (pos)->next)

/// @brief Initializes the list_head.
/// @param head The head for your list.
#define list_head_init(head) \
    (head)->next = (head)->prev = (head)

/// @brief Insert element l2 after l1.
static inline void list_head_insert_after(list_head * l1, list_head * l2)
{
    // [La]->l1  La<-[l1]->Lb    <-[l2]->    l1<-[Lb]

    list_head * l1_next = l1->next;
    // [La]->l1  La<-[l1]->l2    <-[l2]->    l1<-[Lb]
    l1->next = l2;
    // [La]->l1  La<-[l1]->l2  l1<-[l2]->    l1<-[Lb]
    l2->prev = l1;
    // [La]->l1  La<-[l1]->l2  l1<-[l2]->Lb  l1<-[Lb]
    l2->next = l1_next;
    // [La]->l1  La<-[l1]->l2  l1<-[l2]->Lb  l2<-[Lb]
    l1_next->prev = l2;
}

/// @brief Insert element l2 before l1.
static inline void list_head_insert_before(list_head * l1, list_head * l2)
{
    // [La]->l1      [l2]      La<-[l1]->Lb  l1<-[Lb]

    list_head * l1_prev = l1->prev;
    // [La]->l2      [l2]      La<-[l1]->Lb  l1<-[Lb]
    l1_prev->next = l2;
    // [La]->l2  La<-[l2]      La<-[l1]->Lb  l1<-[Lb]
    l2->prev = l1_prev;
    // [La]->l2  La<-[l2]->l1  La<-[l1]->Lb  l1<-[Lb]
    l2->next = l1;
    // [La]->l2  La<-[l2]->l1  l2<-[l1]->Lb  l1<-[Lb]
    l1->prev = l2;
}

/// @brief Remove l from the list.
/// @param l The element to remove.
static inline void list_head_del(list_head * l)
{
    // [La]->l   La<-[l]->Lb  l<-[Lb]

    // [La]->Lb  La<-[l]->Lb  l<-[Lb]
    l->prev->next = l->next;
    // [La]->Lb  La<-[l]->Lb La<-[Lb]
    l->next->prev = l->prev;
    // [La]->Lb   l<-[l]->l  La<-[Lb]
    l->next = l->prev = l;
}

/// @brief Tests whether the given list is empty.
/// @param head The list to check.
/// @return 1 if empty, 0 otherwise.
static inline int list_head_empty(list_head const * head)
{
    return head->next == head;
}

/// Insert a new entry between two known consecutive entries.
static inline void __list_add(list_head * new,
                              list_head * prev,
                              list_head * next)
{
    // [prev]->        <-[new]->        <-[next]

    // [prev]->        <-[new]->     new<-[next]
    next->prev = new;
    // [prev]->        <-[new]->next new<-[next]
    new->next = next;
    // [prev]->    prev<-[new]->next new<-[next]
    new->prev = prev;
    // [prev]->new prev<-[new]->next new<-[next]
    prev->next = new;
}

/// @brief Insert element l2 before l1.
static inline void list_head_add(list_head * new, list_head * head)
{
    __list_add(new, head, head->next);
}

/// @brief Insert element l2 before l1.
static inline void list_head_add_tail(list_head * new, list_head * head)
{
    __list_add(new, head->prev, head);
}
