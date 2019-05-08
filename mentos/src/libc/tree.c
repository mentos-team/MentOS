///                MentOS, The Mentoring Operating system project
/// @file tree.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "tree.h"
#include "list.h"
#include "stdlib.h"
#include "debug.h"
#include "panic.h"

tree_t *tree_create()
{
	tree_t *out = malloc(sizeof(tree_t));
	out->nodes = 0;
	out->root = NULL;

	return out;
}

tree_node_t *tree_set_root(tree_t *tree, void *value)
{
	tree_node_t *root = tree_node_create(value);
	tree->root = root;
	tree->nodes = 1;

	return root;
}

void tree_node_destroy(tree_node_t *node)
{
	listnode_foreach(child, node->children)
	{
		tree_node_destroy((tree_node_t *)child->value);
	}
	free(node->value);
}

void tree_destroy(tree_t *tree)
{
	if (tree->root) {
		tree_node_destroy(tree->root);
	}
}

/// @brief Free a node and its children, but not their contents.
static void tree_node_free(tree_node_t *node)
{
	if (!node) {
		return;
	}
	listnode_foreach(child, node->children)
	{
		tree_node_free(child->value);
	}
	list_destroy(node->children);
	free(node);
}

void tree_free(tree_t *tree)
{
	tree_node_free(tree->root);
}

tree_node_t *tree_node_create(void *value)
{
	tree_node_t *out = malloc(sizeof(tree_node_t));
	out->value = value;
	out->children = list_create();
	out->parent = NULL;

	return out;
}

void tree_node_insert_child_node(tree_t *tree, tree_node_t *parent,
								 tree_node_t *node)
{
	list_insert_back(parent->children, node);
	node->parent = parent;
	tree->nodes++;
}

tree_node_t *tree_node_insert_child(tree_t *tree, tree_node_t *parent,
									void *value)
{
	tree_node_t *out = tree_node_create(value);
	tree_node_insert_child_node(tree, parent, out);

	return out;
}

tree_node_t *tree_node_find_parent(tree_node_t *haystack, tree_node_t *needle)
{
	tree_node_t *found = NULL;
	listnode_foreach(child, haystack->children)
	{
		if (child->value == needle) {
			return haystack;
		}
		found = tree_node_find_parent((tree_node_t *)child->value, needle);
		if (found) {
			break;
		}
	}

	return found;
}

/// @brief Return the parent of a node, inefficiently.
tree_node_t *tree_find_parent(tree_t *tree, tree_node_t *node)
{
	if (!tree->root) {
		return NULL;
	}

	return tree_node_find_parent(tree->root, node);
}

/// @brief Return the number of children this node has.
static size_t tree_count_children(tree_node_t *node)
{
	if (!node) {
		return 0;
	}

	if (!node->children) {
		return 0;
	}

	size_t out = node->children->size;
	listnode_foreach(child, node->children)
	{
		out += tree_count_children((tree_node_t *)child->value);
	}

	return out;
}

void tree_node_parent_remove(tree_t *tree, tree_node_t *parent,
							 tree_node_t *node)
{
	tree->nodes -= tree_count_children(node) + 1;
	list_remove_node(parent->children, list_find(parent->children, node));
	tree_node_free(node);
}

void tree_node_remove(tree_t *tree, tree_node_t *node)
{
	tree_node_t *parent = node->parent;
	if (!parent) {
		if (node == tree->root) {
			tree->nodes = 0;
			tree->root = NULL;
			tree_node_free(node);
		} else
			kernel_panic("Found node with no parent which is not root.");
	}
	tree_node_parent_remove(tree, parent, node);
}

void tree_remove(tree_t *tree, tree_node_t *node)
{
	tree_node_t *parent = node->parent;
	/* This is something we just can't do. We don't know how to merge our
     * children into our "parent" because then we'd have more than one root node.
     * A good way to think about this is actually what this tree struct
     * primarily exists for: processes. Trying to remove the root is equivalent
     * to trying to kill init! Which is bad. We immediately fault on such
     * a case anyway ("Tried to kill init, shutting down!").
     */
	if (!parent) {
		return;
	}
	tree->nodes--;
	list_remove_node(parent->children, list_find(parent->children, node));
	listnode_foreach(child, node->children)
	{
		// Reassign the parents.
		((tree_node_t *)child->value)->parent = parent;
	}
	list_merge(parent->children, node->children);
	free(node);
}

void tree_break_off(tree_t *tree, tree_node_t *node)
{
	tree_node_t *parent = node->parent;
	if (!parent) {
		return;
	}
	list_remove_node(parent->children, list_find(parent->children, node));
}

/// @brief Searches the item inside tree and returns the node which contains it.
tree_node_t *tree_node_find(tree_node_t *node, void *search,
							tree_comparator_t comparator)
{
	if (comparator(node->value, search)) {
		return node;
	}
	tree_node_t *found;
	listnode_foreach(child, node->children)
	{
		found = tree_node_find((tree_node_t *)child->value, search, comparator);
		if (found) {
			return found;
		}
	}

	return NULL;
}

tree_node_t *tree_find(tree_t *tree, void *value, tree_comparator_t comparator)
{
	return tree_node_find(tree->root, value, comparator);
}
