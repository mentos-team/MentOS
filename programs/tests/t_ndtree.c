/// @file t_ndtree.c
/// @brief This program tests N-Dimensional trees.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "ndtree.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Custom allocator for tree nodes
ndtree_node_t *custom_alloc_node(void *value)
{
    ndtree_node_t *node = (ndtree_node_t *)malloc(sizeof(ndtree_node_t));
    if (node) {
        node->value = value;
    }
    return node;
}

// Custom deallocator for tree nodes
void custom_free_node(ndtree_node_t *node) { free(node); }

// Comparison function for integer values
int compare_node(void *lhs, void *rhs) { return (*(int *)lhs) - (*(int *)rhs); }

// Function to print node values (assuming integer values for simplicity)
void print_node(ndtree_node_t *node) { printf("Node value: %d\n", *(int *)(node->value)); }

int main(void)
{
    // Initialize the tree
    ndtree_t tree;
    ndtree_tree_init(&tree, compare_node, custom_alloc_node, custom_free_node);

    // Create the root node
    int root_value      = 1;
    ndtree_node_t *root = ndtree_create_root(&tree, &root_value);
    if (!root) {
        fprintf(stderr, "Error: Failed to create root node\n");
        return 1;
    }

    // Add child nodes to the root (level 2)
    int child1_value = 2, child2_value = 3, child3_value = 4;
    ndtree_node_t *child1 = ndtree_create_child_of_node(&tree, root, &child1_value);
    ndtree_node_t *child2 = ndtree_create_child_of_node(&tree, root, &child2_value);
    ndtree_node_t *child3 = ndtree_create_child_of_node(&tree, root, &child3_value);
    if (!child1 || !child2 || !child3) {
        fprintf(stderr, "Error: Failed to create one or more child nodes for root\n");
        return 1;
    }

    // Add child nodes to child1 (level 3)
    int child1_1_value = 5, child1_2_value = 6;
    if (!ndtree_create_child_of_node(&tree, child1, &child1_1_value) ||
        !ndtree_create_child_of_node(&tree, child1, &child1_2_value)) {
        fprintf(stderr, "Error: Failed to create one or more child nodes for child1\n");
        return 1;
    }

    // Add child nodes to child2 (level 3)
    int child2_1_value = 7, child2_2_value = 8;
    if (!ndtree_create_child_of_node(&tree, child2, &child2_1_value) ||
        !ndtree_create_child_of_node(&tree, child2, &child2_2_value)) {
        fprintf(stderr, "Error: Failed to create one or more child nodes for child2\n");
        return 1;
    }

    // Add child nodes to child3 (level 3)
    int child3_1_value = 9, child3_2_value = 10;
    if (!ndtree_create_child_of_node(&tree, child3, &child3_1_value) ||
        !ndtree_create_child_of_node(&tree, child3, &child3_2_value)) {
        fprintf(stderr, "Error: Failed to create one or more child nodes for child3\n");
        return 1;
    }

    // Verify the number of children at each level
    if (ndtree_node_count_children(root) != 3) {
        fprintf(stderr, "Error: Expected root to have 3 children\n");
        return 1;
    }
    if (ndtree_node_count_children(child1) != 2) {
        fprintf(stderr, "Error: Expected child1 to have 2 children\n");
        return 1;
    }
    if (ndtree_node_count_children(child2) != 2) {
        fprintf(stderr, "Error: Expected child2 to have 2 children\n");
        return 1;
    }
    if (ndtree_node_count_children(child3) != 2) {
        fprintf(stderr, "Error: Expected child3 to have 2 children\n");
        return 1;
    }

    // Traverse the tree and print values using the visitor function
    ndtree_tree_visitor(&tree, print_node, NULL);

    // Test removing a node with children and verify
    ndtree_tree_remove_node(&tree, child2, NULL);
    if (ndtree_node_count_children(root) != 2) {
        fprintf(stderr, "Error: Expected root to have 2 children after removing child2\n");
        return 1;
    }

    // Final deallocation of the tree
    ndtree_tree_dealloc(&tree, NULL);

    return 0;
}
