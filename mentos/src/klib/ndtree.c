/// @file ndtree.c
/// @brief Red/Black tree.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "io/debug.h"
#include "klib/ndtree.h"
#include "assert.h"
#include "klib/list_head.h"
#include "mem/slab.h"

// ============================================================================
// Tree types.

/// @brief Stores data about an NDTree node.
struct ndtree_node_t {
    /// User provided, used indirectly via ndtree_tree_cmp_f.
    void *value;
    /// Pointer to the parent.
    ndtree_node_t *parent;
    /// List of siblings.
    list_head siblings;
    /// List of children.
    list_head children;
};

/// @brief Stores data about an NDTree.
struct ndtree_t {
    /// Comparison function.
    ndtree_tree_cmp_f cmp;
    /// Size of the tree.
    size_t size;
    /// Pointer to the root node.
    ndtree_node_t *root;
    /// List of orphans.
    list_head orphans;
};

/// @brief Stores data about an NDTree iterator.
struct ndtree_iter_t {
    /// Pointer to the head of the list.
    list_head *head;
    /// Pointer to the current element of the list.
    list_head *current;
};

// ============================================================================
// Default Comparison functions.
static inline int __ndtree_tree_node_cmp_ptr_cb(ndtree_t *self, void *a, void *b)
{
    return (a > b) - (a < b);
}

// ============================================================================
// Node management functions.

ndtree_node_t *ndtree_node_alloc()
{
    return kmalloc(sizeof(ndtree_node_t));
}

ndtree_node_t *ndtree_node_create(void *value)
{
    ndtree_node_t *node = ndtree_node_alloc();
    node                = ndtree_node_init(node, value);
    return node;
}

ndtree_node_t *ndtree_node_init(ndtree_node_t *node, void *value)
{
    if (node) {
        node->value  = value;
        node->parent = NULL;
        list_head_init(&node->siblings);
        list_head_init(&node->children);
    }
    return node;
}

void ndtree_node_set_value(ndtree_node_t *node, void *value)
{
    if (node && value) {
        node->value = value;
    }
}

void *ndtree_node_get_value(ndtree_node_t *node)
{
    if (node)
        return node->value;
    return NULL;
}

void ndtree_set_root(ndtree_t *tree, ndtree_node_t *node)
{
    tree->root = node;
    ++tree->size;
}

ndtree_node_t *ndtree_create_root(ndtree_t *tree, void *value)
{
    ndtree_node_t *node = ndtree_node_create(value);
    ndtree_set_root(tree, node);
    return node;
}

ndtree_node_t *ndtree_get_root(ndtree_t *tree)
{
    return tree->root;
}

void ndtree_add_child_to_node(ndtree_t *tree, ndtree_node_t *parent, ndtree_node_t *child)
{
    child->parent = parent;
    list_head_insert_after(&child->siblings, &parent->children);
    ++tree->size;
}

ndtree_node_t *ndtree_create_child_of_node(ndtree_t *tree, ndtree_node_t *parent, void *value)
{
    ndtree_node_t *child = ndtree_node_create(value);
    ndtree_add_child_to_node(tree, parent, child);
    return child;
}

unsigned int ndtree_node_count_children(ndtree_node_t *node)
{
    unsigned int children = 0;
    if (node) {
        list_for_each_decl(it, &node->children)
        {
            ++children;
        }
    }
    return children;
}

void ndtree_node_dealloc(ndtree_node_t *node)
{
    if (node)
        kfree(node);
}

// ============================================================================
// Tree management functions.
ndtree_t *ndtree_tree_alloc()
{
    return kmalloc(sizeof(ndtree_t));
}

ndtree_t *ndtree_tree_create(ndtree_tree_cmp_f cmp)
{
    return ndtree_tree_init(ndtree_tree_alloc(), cmp);
}

ndtree_t *ndtree_tree_init(ndtree_t *tree, ndtree_tree_cmp_f node_cmp_cb)
{
    if (tree) {
        tree->size = 0;
        tree->cmp  = node_cmp_cb ? node_cmp_cb : __ndtree_tree_node_cmp_ptr_cb;
        tree->root = NULL;
    }
    return tree;
}

static void __ndtree_tree_dealloc_rec(ndtree_t *tree, ndtree_node_t *node, ndtree_tree_node_f node_cb)
{
    if (node && node_cb) {
        if (!list_head_empty(&node->children)) {
            list_head *it_save;
            list_for_each_decl(it, &node->children)
            {
                ndtree_node_t *entry = list_entry(it, ndtree_node_t, siblings);
                it_save              = it->prev;
                list_head_remove(it);
                it = it_save;
                __ndtree_tree_dealloc_rec(tree, entry, node_cb);
            }
        }
        node_cb(tree, node);
        kfree(node);
    }
}

void ndtree_tree_dealloc(ndtree_t *tree, ndtree_tree_node_f node_cb)
{
    if (tree && tree->root && node_cb)
        __ndtree_tree_dealloc_rec(tree, tree->root, node_cb);
    kfree(tree);
}

static ndtree_node_t *__ndtree_tree_find_rec(ndtree_t *tree, ndtree_tree_cmp_f cmp, void *value, ndtree_node_t *node)
{
    ndtree_node_t *result = NULL;
    if (tree && cmp && node && value) {
        if (cmp(tree, node->value, value) == 0) {
            result = node;
        } else if (!list_head_empty(&node->children)) {
            list_for_each_decl(it, &node->children)
            {
                ndtree_node_t *child = list_entry(it, ndtree_node_t, siblings);
                if ((result = __ndtree_tree_find_rec(tree, cmp, value, child)) != NULL) {
                    break;
                }
            }
        }
    }
    return result;
}

ndtree_node_t *ndtree_tree_find(ndtree_t *tree, ndtree_tree_cmp_f cmp, void *value)
{
    if (tree && tree->root && value)
        return __ndtree_tree_find_rec(tree, cmp, value, tree->root);
    return NULL;
}

ndtree_node_t *ndtree_node_find(ndtree_t *tree, ndtree_node_t *node, ndtree_tree_cmp_f cmp, void *value)
{
    if (tree && node && value) {
        // Check only if the node has children.
        if (!list_head_empty(&node->children)) {
            // Check which compare function we need to use.
            ndtree_tree_cmp_f cmp_fun = cmp ? cmp : tree->cmp;
            // If neither the tree nor the function argument are valid, rollback to the
            // default comparison function.
            if (cmp_fun == NULL)
                cmp_fun = __ndtree_tree_node_cmp_ptr_cb;
            // Iterate throught the children.
            list_for_each_decl(it, &node->children)
            {
                ndtree_node_t *child = list_entry(it, ndtree_node_t, siblings);
                if (cmp_fun(tree, child->value, value) == 0)
                    return child;
            }
        }
    }
    return NULL;
}

unsigned int ndtree_tree_size(ndtree_t *tree)
{
    if (tree)
        return tree->size;
    return 0;
}

int ndtree_tree_remove_node_with_cb(ndtree_t *tree, ndtree_node_t *node, ndtree_tree_node_f node_cb)
{
    if (tree && node) {
        // Remove the node from the parent list.
        list_head_remove(&node->siblings);
        // If the node has children, we need to migrate them.
        if (!list_head_empty(&node->children)) {
            // The new parent, by default it is NULL.
            ndtree_node_t *new_parent = NULL;
            // The new list, by default it is the list of orphans of the tree.
            list_head *new_list = &tree->orphans;
            // If the found node has a parent, we need to set the variables
            // so that we can migrate the children.
            if (node->parent) {
                new_parent = node->parent;
                new_list   = &node->parent->children;
            }
            // Migrate the children.
            list_for_each_decl(it, &node->children)
            {
                ndtree_node_t *child = list_entry(it, ndtree_node_t, siblings);
                child->parent        = new_parent;
            }
            // Merge the lists.
            list_head_append(new_list, &node->children);
        }
        if (node_cb)
            node_cb(tree, node);
        else
            ndtree_node_dealloc(node);
        --tree->size;
        return 1;
    }
    return 0;
}

int ndtree_tree_remove_with_cb(ndtree_t *tree, void *value, ndtree_tree_node_f node_cb)
{
    if (tree && value) {
        ndtree_node_t *node = ndtree_tree_find(tree, tree->cmp, value);
        return ndtree_tree_remove_node_with_cb(tree, node, node_cb);
    }
    return 0;
}

// ============================================================================
// Iterators.
ndtree_iter_t *ndtree_iter_alloc()
{
    ndtree_iter_t *iter = kmalloc(sizeof(ndtree_iter_t));
    iter->head          = NULL;
    iter->current       = NULL;
    return iter;
}

void ndtree_iter_dealloc(ndtree_iter_t *iter)
{
    if (iter)
        kfree(iter);
}

ndtree_node_t *ndtree_iter_first(ndtree_node_t *node, ndtree_iter_t *iter)
{
    if (node && iter) {
        if (!list_head_empty(&node->children)) {
            iter->head    = &node->children;
            iter->current = iter->head->next;
            return list_entry(iter->current, ndtree_node_t, siblings);
        }
    }
    return NULL;
}

ndtree_node_t *ndtree_iter_last(ndtree_node_t *node, ndtree_iter_t *iter)
{
    if (node && iter) {
        if (!list_head_empty(&node->children)) {
            iter->head    = &node->children;
            iter->current = iter->head->prev;
            return list_entry(iter->current, ndtree_node_t, siblings);
        }
    }
    return NULL;
}

ndtree_node_t *ndtree_iter_next(ndtree_iter_t *iter)
{
    if (iter) {
        if (iter->current->next != iter->head) {
            iter->current = iter->current->next;
            return list_entry(iter->current, ndtree_node_t, siblings);
        }
    }
    return NULL;
}

ndtree_node_t *ndtree_iter_prev(ndtree_iter_t *iter)
{
    if (iter) {
        if (iter->current->next != iter->head) {
            iter->current = iter->current->prev;
            return list_entry(iter->current, ndtree_node_t, siblings);
        }
    }
    return NULL;
}

// ============================================================================
// Tree debugging functions.
static void __ndtree_tree_visitor_iter(ndtree_t *tree,
                                       ndtree_node_t *node,
                                       ndtree_tree_node_f enter_fun,
                                       ndtree_tree_node_f exit_fun)
{
    assert(tree);
    assert(node);
    if (enter_fun)
        enter_fun(tree, node);
    if (!list_head_empty(&node->children))
        list_for_each_decl(it, &node->children)
            __ndtree_tree_visitor_iter(tree, list_entry(it, ndtree_node_t, siblings), enter_fun, exit_fun);
    if (exit_fun)
        exit_fun(tree, node);
}

void ndtree_tree_visitor(ndtree_t *tree, ndtree_tree_node_f enter_fun, ndtree_tree_node_f exit_fun)
{
    if (tree && tree->root)
        __ndtree_tree_visitor_iter(tree, tree->root, enter_fun, exit_fun);
}
