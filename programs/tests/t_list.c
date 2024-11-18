/// @file t_list.c
/// @brief This program tests lists.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h" // Include the list.h header file here

// Custom allocation and deallocation functions for list nodes
static inline listnode_t *node_alloc(void)
{
    listnode_t *node = (listnode_t *)malloc(sizeof(listnode_t));
    assert(node && "Failed to allocate node.");
    memset(node, 0, sizeof(listnode_t));
    return node;
}

void node_dealloc(listnode_t *node)
{
    assert(node && "Invalid node pointer.");
    free(node);
}

int main()
{
    // Initialize lists with custom alloc/dealloc functions
    list_t list1, list2;
    list_init(&list1, node_alloc, node_dealloc);
    list_init(&list2, node_alloc, node_dealloc);

    // Test insertion at the front and back
    list_insert_front(&list1, "apple");
    list_insert_back(&list1, "banana");
    list_insert_front(&list1, "cherry");
    if (list_size(&list1) != 3) {
        printf("Error: list_insert_front or list_insert_back failed\n");
        return 1;
    }

    // Test peeking at the front and back
    if (strcmp(list_peek_front(&list1), "cherry") != 0) {
        printf("Error: list_peek_front failed\n");
        return 1;
    }
    if (strcmp(list_peek_back(&list1), "banana") != 0) {
        printf("Error: list_peek_back failed\n");
        return 1;
    }

    // Test list size and empty check
    if (list_size(&list1) != 3) {
        printf("Error: list_size failed\n");
        return 1;
    }
    if (list_empty(&list1)) {
        printf("Error: list_empty failed\n");
        return 1;
    }

    // Test remove from front
    char *removed = list_remove_front(&list1);
    if (strcmp(removed, "cherry") != 0) {
        printf("Error: list_remove_front failed\n");
        return 1;
    }
    if (list_size(&list1) != 2) {
        printf("Error: list size after list_remove_front is incorrect\n");
        return 1;
    }

    // Test remove from back
    removed = list_remove_back(&list1);
    if (strcmp(removed, "banana") != 0) {
        printf("Error: list_remove_back failed\n");
        return 1;
    }
    if (list_size(&list1) != 1) {
        printf("Error: list size after list_remove_back is incorrect\n");
        return 1;
    }

    // Test find
    list_insert_back(&list1, "banana");
    listnode_t *found_node = list_find(&list1, "banana");
    if (!(found_node && strcmp(found_node->value, "banana") == 0)) {
        printf("Error: list_find failed\n");
        return 1;
    }

    // Test merging lists
    list_insert_back(&list1, "fig");
    list_insert_back(&list1, "grape");
    list_insert_back(&list2, "honeydew");
    list_insert_back(&list2, "kiwi");
    list_merge(&list1, &list2);
    if (!list_empty(&list2)) {
        printf("Error: list_merge failed; source list is not empty\n");
        return 1;
    }
    if (list_size(&list1) != 6) {
        printf("Error: list size after list_merge is incorrect\n");
        return 1;
    }

    // Test destruction
    list_destroy(&list1);
    list_destroy(&list2);
    if (!list_empty(&list1) || list_size(&list1) != 0) {
        printf("Error: list_destroy failed for list1\n");
        return 1;
    }
    if (!list_empty(&list2) || list_size(&list2) != 0) {
        printf("Error: list_destroy failed for list2\n");
        return 1;
    }

    // If all tests passed, return 0
    return 0;
}
