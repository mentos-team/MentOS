/// @file rbtree.h
/// @brief Red/Black tree.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#ifndef RBTREE_ITER_MAX_HEIGHT
/// Tallest allowable tree to iterate.
#define RBTREE_ITER_MAX_HEIGHT 64
#endif

// ============================================================================
// Opaque types.

/// @brief Node of the tree.
typedef struct rbtree_node_t rbtree_node_t;
/// @brief The tree itself.
typedef struct rbtree_t rbtree_t;
/// @brief Iterator for traversing the tree.
typedef struct rbtree_iter_t rbtree_iter_t;

// ============================================================================
// Comparison functions.
/// @brief Function for comparing elements in the tree.
typedef int (*rbtree_tree_cmp_f)(rbtree_t *tree, rbtree_node_t *a, void *arg);
/// @brief Function for comparing elements in the tree.
typedef int (*rbtree_tree_node_cmp_f)(rbtree_t *tree, rbtree_node_t *a, rbtree_node_t *b);
/// @brief Callback to call on elements of the tree.
typedef void (*rbtree_tree_node_f)(rbtree_t *tree, rbtree_node_t *node);

// ============================================================================
// Node management functions.

/// @brief Allocate memory for a node.
/// @return Pointer to the allocated node.
rbtree_node_t *rbtree_node_alloc();

/// @brief Allocate memory for a node and sets its value.
/// @param value Value to associated node.
/// @return Pointer to the allocated node.
rbtree_node_t *rbtree_node_create(void *value);

/// @brief Initializes the already allocated node.
/// @param node  The node itself.
/// @param value The value associated to the node.
/// @return Pointer to the node itself.
rbtree_node_t *rbtree_node_init(rbtree_node_t *node, void *value);

/// @brief Provides access to the value associated to a node.
/// @param node The node itself.
/// @return The value associated to the node.
void *rbtree_node_get_value(rbtree_node_t *node);

/// @brief Deallocate a node.
/// @param node The node to destroy.
void rbtree_node_dealloc(rbtree_node_t *node);

// ============================================================================
// Tree management functions.

/// @brief Allocate memory for a tree.
/// @return Pointer to the allocated tree.
rbtree_t *rbtree_tree_alloc();

/// @brief Allocate memory for a tree and sets the function used to compare nodes.
/// @param cmp Function used to compare elements of the tree.
/// @return Pointer to the allocated tree.
rbtree_t *rbtree_tree_create(rbtree_tree_node_cmp_f cmp);

/// @brief Initializes the tree.
/// @param tree The tree to initialize.
/// @param cmp  The compare function to associate to the tree.
/// @return Pointer to tree itself.
rbtree_t *rbtree_tree_init(rbtree_t *tree, rbtree_tree_node_cmp_f cmp);

/// @brief Deallocate a node.
/// @param tree    The tree to destroy.
/// @param node_cb The function called on each element of the tree before destroying the tree.
void rbtree_tree_dealloc(rbtree_t *tree, rbtree_tree_node_f node_cb);

/// @brief Searches the node inside the tree with the given value.
/// @param tree  The tree.
/// @param value The value to search.
/// @return Pointer to the value itself.
void *rbtree_tree_find(rbtree_t *tree, void *value);

/// @brief Searches the node inside the tree with the given value.
/// @param tree    The tree.
/// @param cmp_fun The node compare function.
/// @param value   The value to search.
/// @return Pointer to the value itself.
void *rbtree_tree_find_by_value(rbtree_t *tree, rbtree_tree_cmp_f cmp_fun, void *value);

/// @brief Interts the value inside the tree.
/// @param tree  The tree.
/// @param value The value to insert.
/// @return 1 on success, 0 on failure.
int rbtree_tree_insert(rbtree_t *tree, void *value);

/// @brief Removes the value from the given tree.
/// @param tree    The tree.
/// @param value   The value to search.
/// @return Returns 1 if the value was removed, 0 otherwise.
int rbtree_tree_remove(rbtree_t *tree, void *value);

/// @brief Returns the size of the tree.
/// @param tree The tree.
/// @return The size of the tree.
unsigned int rbtree_tree_size(rbtree_t *tree);

/// @brief Interts the value inside the tree.
/// @param tree The tree.
/// @param node The node to insert.
/// @return 1 on success, 0 on failure.
int rbtree_tree_insert_node(rbtree_t *tree, rbtree_node_t *node);

/// @brief Removes the value from the tree.
/// @param tree    The tree.
/// @param value   The value to remove.
/// @param node_cb The callback to call on the node before removing the node.
/// @return 1 on success, 0 on failure.
int rbtree_tree_remove_with_cb(rbtree_t *tree, void *value, rbtree_tree_node_f node_cb);

// ============================================================================
// Iterators.

/// @brief Allocate the memory for the iterator.
/// @return Pointer to the allocated iterator.
rbtree_iter_t *rbtree_iter_alloc();

/// @brief Deallocate the memory for the iterator.
/// @param iter Pointer to the allocated iterator.
/// @return Pointer to the iterator itself.
rbtree_iter_t *rbtree_iter_init(rbtree_iter_t *iter);

/// @brief Allocate the memory for the iterator and initializes it.
/// @return Pointer to the allocated iterator.
rbtree_iter_t *rbtree_iter_create();

/// @brief Deallocate the memory for the iterator.
/// @param iter Pointer to the allocated iterator.
void rbtree_iter_dealloc(rbtree_iter_t *iter);

/// @brief Initializes the iterator the the first element of the tree.
/// @param iter The iterator we want to initialize.
/// @param tree The tree.
/// @return Pointer to the first value of the tree.
void *rbtree_iter_first(rbtree_iter_t *iter, rbtree_t *tree);

/// @brief Initializes the iterator the the last element of the tree.
/// @param iter The iterator we want to initialize.
/// @param tree The tree.
/// @return Pointer to the last value of the tree.
void *rbtree_iter_last(rbtree_iter_t *iter, rbtree_t *tree);

/// @brief Moves the iterator to the next element.
/// @param iter The iterator.
/// @return Pointer to the next element.
void *rbtree_iter_next(rbtree_iter_t *iter);

/// @brief Moves the iterator to the previous element.
/// @param iter The iterator.
/// @return Pointer to the previous element.
void *rbtree_iter_prev(rbtree_iter_t *iter);

// ============================================================================
// Tree debugging functions.

/// @brief Checks the tree.
/// @param tree The tree.
/// @param root The root of the tree.
/// @return 1 on failure, 0 on success.
int rbtree_tree_test(rbtree_t *tree, rbtree_node_t *root);

/// @brief Prints the tree using the provided callback.
/// @param tree The tree.
/// @param fun  The print callback.
void rbtree_tree_print(rbtree_t *tree, rbtree_tree_node_f fun);
