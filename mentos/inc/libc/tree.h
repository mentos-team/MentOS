///                MentOS, The Mentoring Operating system project
/// @file tree.h
/// @brief General-purpose tree implementation.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "list.h"

/// @brief A node of the tree.
typedef struct tree_node_t {
	/// The value stored by the node.
	void *value;
	/// Pointer to the list of childrens.
	list_t *children;
	/// Pointer to the parent.
	struct tree_node_t *parent;
} tree_node_t;

/// @brief Represents the tree.
typedef struct tree_t {
	/// Number of nodes inside the tree.
	size_t nodes;
	/// Pointer to the root of the tree.
	tree_node_t *root;
} tree_t;

typedef uint8_t (*tree_comparator_t)(void *, void *);

/// @brief Creates a new tree.
tree_t *tree_create();

/// @brief Sets the root node for a new tree.
tree_node_t *tree_set_root(tree_t *tree, void *value);

/// @brief Free the contents of a node and its children,
///         but not the nodes themselves.
void tree_node_destroy(tree_node_t *node);

/// @brief Free the contents of a tree, but not the nodes.
void tree_destroy(tree_t *tree);

/// @brief Free all of the nodes in a tree, but not their contents.
void tree_free(tree_t *tree);

/// @brief Create a new tree node pointing to the given value.
tree_node_t *tree_node_create(void *value);

/// @brief Insert a node as a child of parent.
void tree_node_insert_child_node(tree_t *tree, tree_node_t *parent,
								 tree_node_t *node);

/// @brief Insert a (fresh) node as a child of parent.
tree_node_t *tree_node_insert_child(tree_t *tree, tree_node_t *parent,
									void *value);

/// @brief Recursive node part of tree_find_parent.
tree_node_t *tree_node_find_parent(tree_node_t *haystack, tree_node_t *needle);

/// @brief Remove a node when we know its parent; update node counts for the
///        tree.
void tree_node_parent_remove(tree_t *tree, tree_node_t *parent,
							 tree_node_t *node);

/// @brief Remove an entire branch given its root.
void tree_node_remove(tree_t *tree, tree_node_t *node);

/// @brief Remove this node and move its children into its parent's
///        list of children.
void tree_remove(tree_t *tree, tree_node_t *node);

// TODO: doxygen comment.
void tree_break_off(tree_t *tree, tree_node_t *node);

/// @brief Searches the given value inside the tree.
tree_node_t *tree_find(tree_t *tree, void *value, tree_comparator_t comparator);
