/// @file ndtree.h
/// @brief N-Dimensional tree implementation.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "list_head.h"

/// @brief Stores data about an NDTree node.
typedef struct ndtree_node {
    void *value;                ///< User-provided value, used indirectly via ndtree_tree_compare_f.
    struct ndtree_node *parent; ///< Pointer to the parent.
    struct list_head siblings;  ///< List of siblings.
    struct list_head children;  ///< List of children.
} ndtree_node_t;

/// @brief Function pointer type for comparing elements in the tree.
/// @param tree The tree containing the elements to compare.
/// @param lhs Left-hand side value to compare.
/// @param rhs Right-hand side value to compare.
/// @return Comparison result: <0 if lhs < rhs, 0 if lhs == rhs, >0 if lhs > rhs.
typedef int (*ndtree_tree_compare_f)(void *lhs, void *rhs);

/// @brief Callback function type for operating on tree nodes.
/// @param tree The tree containing the node.
/// @param node The node to operate on.
typedef void (*ndtree_tree_node_f)(ndtree_node_t *node);

/// @brief Custom allocator for tree nodes.
/// @param value The value to store in the node.
/// @return Pointer to the allocated node.
typedef ndtree_node_t *(*ndtree_alloc_node_f)(void *value);

/// @brief Custom deallocator for tree nodes.
/// @param node The node to deallocate.
typedef void (*ndtree_free_node_f)(ndtree_node_t *node);

/// @brief Stores data about an NDTree.
typedef struct ndtree {
    unsigned size;                      ///< Size of the tree.
    ndtree_node_t *root;                ///< Pointer to the root node.
    struct list_head orphans;           ///< List of orphans.
    ndtree_tree_compare_f compare_node; ///< Custom node comparison.
    ndtree_alloc_node_f alloc_node;     ///< Custom allocator for nodes.
    ndtree_free_node_f free_node;       ///< Custom deallocator for nodes.
} ndtree_t;

/// @brief Initializes a tree with comparison, allocation, and deallocation functions.
/// @param tree The tree to initialize (already allocated by the user).
/// @param compare_node Comparison function for nodes.
/// @param alloc_node Custom allocator for nodes.
/// @param free_node Custom deallocator for nodes.
/// @return Pointer to the initialized tree.
void ndtree_tree_init(ndtree_t *tree, ndtree_tree_compare_f compare_node, ndtree_alloc_node_f alloc_node, ndtree_free_node_f free_node);

/// @brief Initializes a tree node with a given value.
/// @param node The node to initialize.
/// @param value The value to store in the node.
void ndtree_node_init(ndtree_node_t *node, void *value);

/// @brief Creates a root node for the tree.
/// @param tree The tree to add the root node to.
/// @param value The value for the root node.
/// @return Pointer to the newly created root node, or NULL on failure.
ndtree_node_t *ndtree_create_root(ndtree_t *tree, void *value);

/// @brief Adds a child node to a specified parent node.
/// @param tree The tree containing the nodes.
/// @param parent The parent node.
/// @param child The child node to add.
void ndtree_add_child_to_node(ndtree_t *tree, ndtree_node_t *parent, ndtree_node_t *child);

/// @brief Creates and adds a child node to a specified parent node.
/// @param tree The tree to add the child to.
/// @param parent The parent node.
/// @param value The value for the new child node.
/// @return Pointer to the newly created child node, or NULL on failure.
ndtree_node_t *ndtree_create_child_of_node(ndtree_t *tree, ndtree_node_t *parent, void *value);

/// @brief Counts the number of children of a node.
/// @param node The node to count children for.
/// @return The number of children of the node.
unsigned int ndtree_node_count_children(ndtree_node_t *node);

/// @brief Deallocates all nodes in the tree.
/// @param tree The tree to deallocate.
/// @param node_cb Optional callback to call on each node before deallocation.
void ndtree_tree_dealloc(ndtree_t *tree, ndtree_tree_node_f node_cb);

/// @brief Searches for a node in the tree based on a given value.
/// @param tree The tree to search in.
/// @param value The value to search for.
/// @return Pointer to the found node, or NULL if not found.
ndtree_node_t *ndtree_tree_find(ndtree_t *tree, void *value);

/// @brief Removes a specified node from the tree with an optional callback.
/// @param tree The tree containing the node.
/// @param node The node to remove.
/// @param node_cb Optional callback to call before removing the node.
/// @return 1 if the node was removed, 0 otherwise.
int ndtree_tree_remove_node(ndtree_t *tree, ndtree_node_t *node, ndtree_tree_node_f node_cb);

/// @brief Initiates a recursive visit through all nodes in the tree, calling specified enter and exit functions.
/// @param tree The tree to traverse.
/// @param enter_fun Function to call upon entering each node, or NULL to skip.
/// @param exit_fun Function to call upon exiting each node, or NULL to skip.
void ndtree_tree_visitor(ndtree_t *tree, ndtree_tree_node_f enter_fun, ndtree_tree_node_f exit_fun);
