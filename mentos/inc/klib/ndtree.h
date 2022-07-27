/// @file ndtree.h
/// @brief N-Dimensional tree.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

// ============================================================================
// Opaque types.

/// @brief Node of the tree.
typedef struct ndtree_node_t ndtree_node_t;
/// @brief The tree itself.
typedef struct ndtree_t ndtree_t;
/// @brief Iterator for traversing the tree.
typedef struct ndtree_iter_t ndtree_iter_t;

// ============================================================================
// Comparison functions.

/// @brief Function for comparing elements in the tree.
typedef int (*ndtree_tree_cmp_f)(ndtree_t *tree, void *lhs, void *rhs);
/// @brief Callback to call on elements of the tree.
typedef void (*ndtree_tree_node_f)(ndtree_t *tree, ndtree_node_t *node);

// ============================================================================
// Node management functions.

/// @brief Allocate memory for a node.
/// @return Pointer to the allocated node.
ndtree_node_t *ndtree_node_alloc();

/// @brief Allocate memory for a node and sets its value.
/// @param value Value to associated node.
/// @return Pointer to the allocated node.
ndtree_node_t *ndtree_node_create(void *value);

/// @brief Initializes the already allocated node.
/// @param node  The node itself.
/// @param value The value associated to the node.
/// @return Pointer to the node itself.
ndtree_node_t *ndtree_node_init(ndtree_node_t *node, void *value);

/// @brief Sets the value of the given node.
/// @param node  The node to manipulate.
/// @param value The value associated to the node.
void ndtree_node_set_value(ndtree_node_t *node, void *value);

/// @brief Provides access to the value associated to a node.
/// @param node The node itself.
/// @return The value associated to the node.
void *ndtree_node_get_value(ndtree_node_t *node);

/// @brief Sets the given node as root of the tree.
/// @param tree The tree.
/// @param node The node to set as root.
void ndtree_set_root(ndtree_t *tree, ndtree_node_t *node);

/// @brief Creates a new node and assigns it as root of the tree.
/// @param tree  The tree.
/// @param value The value associated to the node.
/// @return The newly created node.
ndtree_node_t *ndtree_create_root(ndtree_t *tree, void *value);

/// @brief Provides access to the root of the tree.
/// @param tree The tree.
/// @return Pointer to the node.
ndtree_node_t *ndtree_get_root(ndtree_t *tree);

/// @brief Adds the given `child` as child of `parent`.
/// @param tree   The tree.
/// @param parent The `parent` node.
/// @param child  The new `child` node.
void ndtree_add_child_to_node(ndtree_t *tree, ndtree_node_t *parent, ndtree_node_t *child);

/// @brief Creates a new node and sets it as child of `parent`.
/// @param tree   The tree.
/// @param parent The `parent` node.
/// @param value  Value associated with the new child.
/// @return Pointer to the newly created child node.
ndtree_node_t *ndtree_create_child_of_node(ndtree_t *tree, ndtree_node_t *parent, void *value);

/// @brief Counts the number of children of the given node.
/// @param node The node of which we count the children.
/// @return The number of children.
unsigned int ndtree_node_count_children(ndtree_node_t *node);

/// @brief Deallocate a node.
/// @param node The node to destroy.
void ndtree_node_dealloc(ndtree_node_t *node);

// ============================================================================
// Tree management functions.

/// @brief Allocate memory for a tree.
/// @return Pointer to the allocated tree.
ndtree_t *ndtree_tree_alloc();

/// @brief Allocate memory for a tree and sets the function used to compare nodes.
/// @param cmp Function used to compare elements of the tree.
/// @return Pointer to the allocated tree.
ndtree_t *ndtree_tree_create(ndtree_tree_cmp_f cmp);

/// @brief Deallocate a node.
/// @param tree    The tree to destroy.
/// @param node_cb The function called on each element of the tree before destroying the tree.
void ndtree_tree_dealloc(ndtree_t *tree, ndtree_tree_node_f node_cb);

/// @brief Initializes the tree.
/// @param tree The tree to initialize.
/// @param cmp  The compare function to associate to the tree.
/// @return Pointer to tree itself.
ndtree_t *ndtree_tree_init(ndtree_t *tree, ndtree_tree_cmp_f cmp);

/// @brief Searches the node inside the tree with the given value.
/// @param tree  The tree.
/// @param cmp   The node compare function.
/// @param value The value to search.
/// @return Node associated with the value.
ndtree_node_t *ndtree_tree_find(ndtree_t *tree, ndtree_tree_cmp_f cmp, void *value);

/// @brief Searches the given value among the children of node.
/// @param tree  The tree.
/// @param node  The node under which we search.
/// @param cmp   The node compare function.
/// @param value The value to search.
/// @return Node associated with the value.
ndtree_node_t *ndtree_node_find(ndtree_t *tree, ndtree_node_t *node, ndtree_tree_cmp_f cmp, void *value);

/// @brief Returns the size of the tree.
/// @param tree The tree.
/// @return The size of the tree.
unsigned int ndtree_tree_size(ndtree_t *tree);

/// @brief Removes the node from the given tree.
/// @param tree    The tree.
/// @param node    The node to remove.
/// @param node_cb The function called on the node before removing it.
/// @return Returns 1 if the node was removed, 0 otherwise.
/// @details
/// Optionally the node callback can be provided to dealloc node and/or
/// user data. Use ndtree_tree_node_dealloc default callback to deallocate
/// node created by ndtree_tree_insert(...).
int ndtree_tree_remove_node_with_cb(ndtree_t *tree, ndtree_node_t *node, ndtree_tree_node_f node_cb);

/// @brief Removes the node from the given tree.
/// @param tree    The tree.
/// @param value   The value to search.
/// @param node_cb The function called on the node before removing it.
/// @return Returns 1 if the value was removed, 0 otherwise.
/// @details
/// Optionally the node callback can be provided to dealloc node and/or
/// user data. Use ndtree_tree_node_dealloc default callback to deallocate
/// node created by ndtree_tree_insert(...).
int ndtree_tree_remove_with_cb(ndtree_t *tree, void *value, ndtree_tree_node_f node_cb);

// ============================================================================
// Iterators.

/// @brief Allocate the memory for the iterator.
/// @return Pointer to the allocated iterator.
ndtree_iter_t *ndtree_iter_alloc();

/// @brief Deallocate the memory for the iterator.
/// @param iter Pointer to the allocated iterator.
void ndtree_iter_dealloc(ndtree_iter_t *iter);

/// @brief Initializes the iterator the the first child of the node.
/// @param node The node of which we want to iterate the children.
/// @param iter The iterator we want to initialize.
/// @return Pointer to the first node of the list.
ndtree_node_t *ndtree_iter_first(ndtree_node_t *node, ndtree_iter_t *iter);

/// @brief Initializes the iterator the the last child of the node.
/// @param node The node of which we want to iterate the children.
/// @param iter The iterator we want to initialize.
/// @return Pointer to the last node of the list.
ndtree_node_t *ndtree_iter_last(ndtree_node_t *node, ndtree_iter_t *iter);

/// @brief Moves the iterator to the next element.
/// @param iter The iterator.
/// @return Pointer to the next element.
ndtree_node_t *ndtree_iter_next(ndtree_iter_t *iter);

/// @brief Moves the iterator to the previous element.
/// @param iter The iterator.
/// @return Pointer to the previous element.
ndtree_node_t *ndtree_iter_prev(ndtree_iter_t *iter);

// ============================================================================
// Tree visiting functions.

/// @brief Run a visit of the tree (DFS).
/// @param tree      The tree to visit.
/// @param enter_fun Function to call when entering a node.
/// @param exit_fun  Function to call when exiting a node.
void ndtree_tree_visitor(ndtree_t *tree, ndtree_tree_node_f enter_fun, ndtree_tree_node_f exit_fun);
