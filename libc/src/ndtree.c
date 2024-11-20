/// @file ndtree.c
/// @brief Red/Black tree.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "ndtree.h"

#include "assert.h"

// ============================================================================
// Init functions.

void ndtree_tree_init(ndtree_t *tree, ndtree_tree_compare_f compare_node, ndtree_alloc_node_f alloc_node, ndtree_free_node_f free_node)
{
    // Validate that the tree and function pointers are not NULL.
    assert(tree && "ndtree_tree_init: Variable tree is NULL.");
    assert(compare_node && "ndtree_tree_init: Function pointer compare_node is NULL.");
    assert(alloc_node && "ndtree_tree_init: Function pointer alloc_node is NULL.");
    assert(free_node && "ndtree_tree_init: Function pointer free_node is NULL.");

    // Initialize tree properties to default values.
    tree->size         = 0;
    tree->root         = NULL;
    tree->compare_node = compare_node;
    tree->alloc_node   = alloc_node;
    tree->free_node    = free_node;

    // Initialize the orphan list head.
    list_head_init(&tree->orphans);
}

void ndtree_node_init(ndtree_node_t *node, void *value)
{
    // Validate that the node and value are not NULL.
    assert(node && "ndtree_node_init: Variable node is NULL.");
    assert(value && "ndtree_node_init: Variable value is NULL.");

    // Set the node's value and initialize relationships.
    node->value  = value;
    node->parent = NULL;

    // Initialize sibling and children lists.
    list_head_init(&node->siblings);
    list_head_init(&node->children);
}

// ============================================================================
// Node Management Functions

ndtree_node_t *ndtree_create_root(ndtree_t *tree, void *value)
{
    // Ensure the tree and the allocation function are valid.
    assert(tree && "ndtree_create_root: Variable tree is NULL.");
    assert(value && "ndtree_create_root: Variable value is NULL.");

    // Allocate a new node for the root using the custom allocator.
    ndtree_node_t *node = tree->alloc_node(value);
    if (node) {
        // Initialize the node with the provided value.
        ndtree_node_init(node, value);

        // Set this node as the tree's root and update the tree size.
        tree->root = node;
        tree->size = 1;
    }

    // Return the newly created root node, or NULL if allocation failed.
    return node;
}

void ndtree_add_child_to_node(ndtree_t *tree, ndtree_node_t *parent, ndtree_node_t *child)
{
    // Ensure the tree, parent, and child nodes are valid.
    assert(tree && "ndtree_add_child_to_node: Variable tree is NULL.");
    assert(parent && "ndtree_add_child_to_node: Variable parent is NULL.");
    assert(child && "ndtree_add_child_to_node: Variable child is NULL.");

    // Set the parent of the child node.
    child->parent = parent;

    // Insert the child into the parent's list of children.
    list_head_insert_before(&child->siblings, &parent->children);

    // Increment the tree's size to reflect the new node.
    ++tree->size;
}

ndtree_node_t *ndtree_create_child_of_node(ndtree_t *tree, ndtree_node_t *parent, void *value)
{
    // Ensure the tree, allocation function, and parent node are valid.
    assert(tree && "ndtree_create_child_of_node: Variable tree is NULL.");
    assert(parent && "ndtree_create_child_of_node: Variable parent is NULL.");
    assert(value && "ndtree_create_child_of_node: Variable value is NULL.");

    // Allocate a new node for the child using the custom allocator.
    ndtree_node_t *child = tree->alloc_node(value);
    if (child) {
        // Initialize the child node with the provided value.
        ndtree_node_init(child, value);

        // Add the child node to the parent node in the tree structure.
        ndtree_add_child_to_node(tree, parent, child);
    }

    // Return the newly created child node, or NULL if allocation failed.
    return child;
}

unsigned int ndtree_node_count_children(ndtree_node_t *node)
{
    // Ensure the node is valid.
    assert(node && "ndtree_node_count_children: Variable node is NULL.");

    // Return the total count of children.
    return list_head_size(&node->children);
}

// ============================================================================
// Tree Management Functions

/// @brief Recursively deallocates nodes in the tree.
/// @param tree The tree containing the nodes.
/// @param node The current node to deallocate.
/// @param node_cb Optional callback function to invoke before deallocating each node.
static void __ndtree_tree_dealloc_rec(ndtree_t *tree, ndtree_node_t *node, ndtree_tree_node_f node_cb)
{
    // Ensure the tree, node, and free_node function are valid.
    assert(tree && "ndtree_tree_dealloc_rec: Variable tree is NULL.");
    assert(node && "ndtree_tree_dealloc_rec: Variable node is NULL.");

    // Iterate safely over the list of children and recursively deallocate each child.
    list_for_each_safe_decl(it, store, &node->children)
    {
        ndtree_node_t *child = list_entry(it, ndtree_node_t, siblings);
        __ndtree_tree_dealloc_rec(tree, child, node_cb);
    }

    // Invoke the callback function on the current node, if provided.
    if (node_cb) {
        node_cb(node);
    }

    // Deallocate the current node using the custom free function.
    tree->free_node(node);
}

void ndtree_tree_dealloc(ndtree_t *tree, ndtree_tree_node_f node_cb)
{
    // Ensure the tree is valid.
    assert(tree && "ndtree_tree_dealloc: Variable tree is NULL.");

    // Check if the tree has a root node to begin deallocation.
    if (tree->root) {
        // Recursively deallocate all nodes starting from the root.
        __ndtree_tree_dealloc_rec(tree, tree->root, node_cb);

        // Reset the tree's root and size after deallocation.
        tree->root = NULL;
        tree->size = 0;

        // Iterate safely over the list of children and recursively deallocate
        // each orphan.
        list_for_each_safe_decl(it, store, &tree->orphans)
        {
            ndtree_node_t *orphan = list_entry(it, ndtree_node_t, siblings);
            __ndtree_tree_dealloc_rec(tree, orphan, node_cb);
        }
    }
}

// ============================================================================
// Tree Search Functions

/// @brief Recursively searches for a node with a specified value in the tree.
/// @param tree The tree to search in.
/// @param value The value to search for.
/// @param node The current node in the recursion.
/// @return Pointer to the found node if successful, NULL otherwise.
static ndtree_node_t *__ndtree_tree_find_rec(ndtree_t *tree, void *value, ndtree_node_t *node)
{
    // Ensure the tree, comparison function, node, and search value are valid.
    assert(tree && "ndtree_tree_find_rec: Variable tree is NULL.");
    assert(node && "ndtree_tree_find_rec: Variable node is NULL.");
    assert(value && "ndtree_tree_find_rec: Variable value is NULL.");

    // Check if the current node matches the search value using the comparison function.
    if (tree->compare_node(node->value, value) == 0) {
        return node; // Return the node if a match is found.
    }

    // Recursively search in each child of the current node.
    list_for_each_decl(it, &node->children)
    {
        ndtree_node_t *child  = list_entry(it, ndtree_node_t, siblings);
        ndtree_node_t *result = __ndtree_tree_find_rec(tree, value, child);
        if (result) {
            return result; // Return the matching node if found in recursion.
        }
    }

    // Return NULL if no match is found in the subtree.
    return NULL;
}

ndtree_node_t *ndtree_tree_find(ndtree_t *tree, void *value)
{
    // Ensure the tree, comparison function, node, and search value are valid.
    assert(tree && "ndtree_tree_find: Variable tree is NULL.");
    assert(tree->root && "ndtree_tree_find: Variable tree->root is NULL.");
    assert(value && "ndtree_tree_find: Variable value is NULL.");

    return __ndtree_tree_find_rec(tree, value, tree->root);
}

// ============================================================================
// Tree Removal Functions

int ndtree_tree_remove_node(ndtree_t *tree, ndtree_node_t *node, ndtree_tree_node_f node_cb)
{
    // Ensure the tree and node are valid.
    assert(tree && "ndtree_tree_remove_node: Variable tree is NULL.");
    assert(node && "ndtree_tree_remove_node: Variable node is NULL.");

    // Remove the node from its sibling list.
    list_head_remove(&node->siblings);

    // If the node has children, orphan them.
    if (!list_head_empty(&node->children)) {
        list_for_each_decl(it, &node->children)
        {
            ndtree_node_t *child = list_entry(it, ndtree_node_t, siblings);
            child->parent        = NULL;
        }
        list_head_append(&tree->orphans, &node->children);
    }

    // Call the callback function if provided.
    if (node_cb) {
        node_cb(node);
    }

    // Free the node using the custom deallocator.
    tree->free_node(node);

    // Decrement the tree size to reflect the removal.
    --tree->size;

    // Return 1 indicating successful removal.
    return 1;
}

// ============================================================================
// Tree Visit Functions

/// @brief Recursively visits each node in the tree, calling specified enter and exit functions.
/// @param tree The tree containing the nodes to visit.
/// @param node The current node being visited.
/// @param enter_fun Function to call upon entering each node, or NULL to skip.
/// @param exit_fun Function to call upon exiting each node, or NULL to skip.
static void __ndtree_tree_visitor_rec(ndtree_t *tree, ndtree_node_t *node, ndtree_tree_node_f enter_fun, ndtree_tree_node_f exit_fun)
{
    // Ensure that the tree and node are valid.
    assert(tree && "ndtree_tree_visitor_rec: Variable tree is NULL.");
    assert(node && "ndtree_tree_visitor_rec: Variable node is NULL.");

    // Call the enter function, if provided.
    if (enter_fun) {
        enter_fun(node);
    }

    // Recursively visit each child node.
    if (!list_head_empty(&node->children)) {
        list_for_each_decl(it, &node->children)
        {
            __ndtree_tree_visitor_rec(tree, list_entry(it, ndtree_node_t, siblings), enter_fun, exit_fun);
        }
    }

    // Call the exit function, if provided, after all children are visited.
    if (exit_fun) {
        exit_fun(node);
    }
}

void ndtree_tree_visitor(ndtree_t *tree, ndtree_tree_node_f enter_fun, ndtree_tree_node_f exit_fun)
{
    // Ensure the tree is valid.
    assert(tree && "ndtree_tree_visitor: Variable tree is NULL.");

    // Start the recursive visitor from the root node, if it exists.
    if (tree->root) {
        __ndtree_tree_visitor_rec(tree, tree->root, enter_fun, exit_fun);
    }
}
