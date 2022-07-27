/// @file rbtree.c
/// @brief Red/Black tree.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "klib/rbtree.h"

#include "assert.h"
#include "io/debug.h"
#include "mem/slab.h"

/// @brief Stores information of a node.
struct rbtree_node_t {
    /// Color red (1), black (0)
    int red;
    /// Link left [0] and right [1]
    rbtree_node_t *link[2];
    /// User provided, used indirectly via rbtree_tree_node_cmp_f.
    void *value;
};

/// @brief Stores information of a rbtree.
struct rbtree_t {
    /// Root of the tree.
    rbtree_node_t *root;
    /// Comparison function for insertion.
    rbtree_tree_node_cmp_f cmp;
    /// Size of the tree.
    unsigned int size;
};

/// @brief Stores information for iterating a rbtree.
struct rbtree_iter_t {
    /// Pointer to the tree itself.
    rbtree_t *tree;
    /// Current node
    rbtree_node_t *node;
    /// Traversal path
    rbtree_node_t *path[RBTREE_ITER_MAX_HEIGHT];
    /// Top of stack
    unsigned int top;
};

rbtree_node_t *rbtree_node_alloc()
{
    return kmalloc(sizeof(rbtree_node_t));
}

rbtree_node_t *rbtree_node_init(rbtree_node_t *node, void *value)
{
    if (node) {
        node->red     = 1;
        node->link[0] = node->link[1] = NULL;
        node->value                   = value;
    }
    return node;
}

rbtree_node_t *rbtree_node_create(void *value)
{
    return rbtree_node_init(rbtree_node_alloc(), value);
}

void *rbtree_node_get_value(rbtree_node_t *node)
{
    if (node)
        return node->value;
    return NULL;
}

void rbtree_node_dealloc(rbtree_node_t *node)
{
    if (node)
        kfree(node);
}

static int rbtree_node_is_red(const rbtree_node_t *node)
{
    return node ? node->red : 0;
}

static rbtree_node_t *rbtree_node_rotate(rbtree_node_t *node, int dir)
{
    rbtree_node_t *result = NULL;
    if (node) {
        result            = node->link[!dir];
        node->link[!dir]  = result->link[dir];
        result->link[dir] = node;
        node->red         = 1;
        result->red       = 0;
    }
    return result;
}

static rbtree_node_t *rbtree_node_rotate2(rbtree_node_t *node, int dir)
{
    rbtree_node_t *result = NULL;
    if (node) {
        node->link[!dir] = rbtree_node_rotate(node->link[!dir], !dir);
        result           = rbtree_node_rotate(node, dir);
    }
    return result;
}

// rbtree_t - default callbacks

static int rbtree_tree_node_cmp_ptr_cb(
    rbtree_t *tree,
    rbtree_node_t *a,
    rbtree_node_t *b)
{
    (void)tree;
    return (a->value > b->value) - (a->value < b->value);
}

static void rbtree_tree_node_dealloc_cb(rbtree_t *tree, rbtree_node_t *node)
{
    if (tree)
        if (node)
            rbtree_node_dealloc(node);
}

// rbtree_t

rbtree_t *rbtree_tree_alloc()
{
    return kmalloc(sizeof(rbtree_t));
}

rbtree_t *rbtree_tree_init(rbtree_t *tree, rbtree_tree_node_cmp_f node_cmp_cb)
{
    if (tree) {
        tree->root = NULL;
        tree->size = 0;
        tree->cmp  = node_cmp_cb ? node_cmp_cb : rbtree_tree_node_cmp_ptr_cb;
    }
    return tree;
}

rbtree_t *rbtree_tree_create(rbtree_tree_node_cmp_f node_cb)
{
    return rbtree_tree_init(rbtree_tree_alloc(), node_cb);
}

void rbtree_tree_dealloc(rbtree_t *tree, rbtree_tree_node_f node_cb)
{
    assert(tree);
    if (node_cb) {
        rbtree_node_t *node = tree->root;
        rbtree_node_t *save = NULL;

        // Rotate away the left links so that
        // we can treat this like the destruction
        // of a linked list
        while (node) {
            if (node->link[0] == NULL) {
                // No left links, just kill the node and move on
                save = node->link[1];
                node_cb(tree, node);
                kfree(node);
                node = NULL;
            } else {
                // Rotate away the left link and check again
                save          = node->link[0];
                node->link[0] = save->link[1];
                save->link[1] = node;
            }
            node = save;
        }
    }
    kfree(tree);
}

void *rbtree_tree_find(rbtree_t *tree, void *value)
{
    void *result = NULL;
    if (tree) {
        rbtree_node_t node = { .value = value };
        rbtree_node_t *it  = tree->root;
        int cmp            = 0;
        while (it) {
            if ((cmp = tree->cmp(tree, it, &node))) {
                // If the tree supports duplicates, they should be
                // chained to the right subtree for this to work
                it = it->link[cmp < 0];
            } else {
                break;
            }
        }
        result = it ? it->value : NULL;
    }
    return result;
}

void *rbtree_tree_find_by_value(rbtree_t *tree,
                                rbtree_tree_cmp_f cmp_fun,
                                void *value)
{
    void *result = NULL;
    if (tree) {
        rbtree_node_t *it = tree->root;
        int cmp           = 0;
        while (it) {
            if ((cmp = cmp_fun(tree, it, value))) {
                // If the tree supports duplicates, they should be
                // chained to the right subtree for this to work
                it = it->link[cmp < 0];
            } else {
                break;
            }
        }
        result = it ? it->value : NULL;
    }
    return result;
}

// Creates (kmalloc'ates)
int rbtree_tree_insert(rbtree_t *tree, void *value)
{
    return rbtree_tree_insert_node(tree, rbtree_node_create(value));
}

// Returns 1 on success, 0 otherwise.
int rbtree_tree_insert_node(rbtree_t *tree, rbtree_node_t *node)
{
    if (tree && node) {
        if (tree->root == NULL) {
            tree->root = node;
        } else {
            rbtree_node_t head = { 0 }; // False tree root
            rbtree_node_t *g, *t;       // Grandparent & parent
            rbtree_node_t *p, *q;       // Iterator & parent
            int dir = 0, last = 0;

            // Set up our helpers
            t = &head;
            g = p = NULL;
            q = t->link[1] = tree->root;

            // Search down the tree for a place to insert
            while (1) {
                if (q == NULL) {
                    // Insert node at the first null link.
                    p->link[dir] = q = node;
                } else if (rbtree_node_is_red(q->link[0]) &&
                           rbtree_node_is_red(q->link[1])) {
                    // Simple red violation: color flip
                    q->red          = 1;
                    q->link[0]->red = 0;
                    q->link[1]->red = 0;
                }

                if (rbtree_node_is_red(q) && rbtree_node_is_red(p)) {
                    // Hard red violation: rotations necessary
                    int dir2 = t->link[1] == g;
                    if (q == p->link[last]) {
                        t->link[dir2] = rbtree_node_rotate(g, !last);
                    } else {
                        t->link[dir2] = rbtree_node_rotate2(g, !last);
                    }
                }

                // Stop working if we inserted a node. This
                // check also disallows duplicates in the tree
                if (tree->cmp(tree, q, node) == 0) {
                    break;
                }

                last = dir;
                dir  = tree->cmp(tree, q, node) < 0;

                // Move the helpers down
                if (g != NULL) {
                    t = g;
                }

                g = p, p = q;
                q = q->link[dir];
            }

            // Update the root (it may be different)
            tree->root = head.link[1];
        }

        // Make the root black for simplified logic
        tree->root->red = 0;
        ++tree->size;
    }

    return 1;
}

// Returns 1 if the value was removed, 0 otherwise. Optional node callback
// can be provided to dealloc node and/or user data. Use rbtree_tree_node_dealloc
// default callback to deallocate node created by rbtree_tree_insert(...).
int rbtree_tree_remove_with_cb(rbtree_t *tree,
                               void *value,
                               rbtree_tree_node_f node_cb)
{
    if (tree->root != NULL) {
        rbtree_node_t head = { 0 };              // False tree root
        rbtree_node_t node = { .value = value }; // Value wrapper node
        rbtree_node_t *q, *p, *g;                // Helpers
        rbtree_node_t *f = NULL;                 // Found item
        int dir          = 1;

        // Set up our helpers
        q = &head;
        g = p      = NULL;
        q->link[1] = tree->root;

        // Search and push a red node down
        // to fix red violations as we go
        while (q->link[dir] != NULL) {
            int last = dir;

            // Move the helpers down
            g = p, p = q;
            q   = q->link[dir];
            dir = tree->cmp(tree, q, &node) < 0;

            // Save the node with matching value and keep
            // going; we'll do removal tasks at the end
            if (tree->cmp(tree, q, &node) == 0) {
                f = q;
            }

            // Push the red node down with rotations and color flips
            if (!rbtree_node_is_red(q) && !rbtree_node_is_red(q->link[dir])) {
                if (rbtree_node_is_red(q->link[!dir])) {
                    p = p->link[last] = rbtree_node_rotate(q, dir);
                } else if (!rbtree_node_is_red(q->link[!dir])) {
                    rbtree_node_t *s = p->link[!last];
                    if (s) {
                        if (!rbtree_node_is_red(s->link[!last]) &&
                            !rbtree_node_is_red(s->link[last])) {
                            // Color flip
                            p->red = 0;
                            s->red = 1;
                            q->red = 1;
                        } else {
                            int dir2 = g->link[1] == p;
                            if (rbtree_node_is_red(s->link[last])) {
                                g->link[dir2] = rbtree_node_rotate2(p, last);
                            } else if (rbtree_node_is_red(s->link[!last])) {
                                g->link[dir2] = rbtree_node_rotate(p, last);
                            }

                            // Ensure correct coloring
                            q->red = g->link[dir2]->red = 1;
                            g->link[dir2]->link[0]->red = 0;
                            g->link[dir2]->link[1]->red = 0;
                        }
                    }
                }
            }
        }

        // Replace and remove the saved node
        if (f) {
            void *tmp = f->value;
            f->value  = q->value;
            q->value  = tmp;

            p->link[p->link[1] == q] = q->link[q->link[0] == NULL];

            if (node_cb) {
                node_cb(tree, q);
            }
            q = NULL;
        }

        // Update the root (it may be different)
        tree->root = head.link[1];

        // Make the root black for simplified logic
        if (tree->root != NULL) {
            tree->root->red = 0;
        }

        --tree->size;
    }
    return 1;
}

int rbtree_tree_remove(rbtree_t *tree, void *value)
{
    int result = 0;
    if (tree) {
        result = rbtree_tree_remove_with_cb(
            tree, value, rbtree_tree_node_dealloc_cb);
    }
    return result;
}

unsigned int rbtree_tree_size(rbtree_t *tree)
{
    unsigned int result = 0;
    if (tree) {
        result = tree->size;
    }
    return result;
}

// rbtree_iter_t

rbtree_iter_t *rbtree_iter_alloc()
{
    return kmalloc(sizeof(rbtree_iter_t));
}

rbtree_iter_t *rbtree_iter_init(rbtree_iter_t *iter)
{
    if (iter) {
        iter->tree = NULL;
        iter->node = NULL;
        iter->top  = 0;
    }
    return iter;
}

rbtree_iter_t *rbtree_iter_create()
{
    return rbtree_iter_init(rbtree_iter_alloc());
}

void rbtree_iter_dealloc(rbtree_iter_t *iter)
{
    if (iter) {
        kfree(iter);
    }
}

// Internal function, init traversal object, dir determines whether
// to begin traversal at the smallest or largest valued node.
static void *rbtree_iter_start(rbtree_iter_t *iter, rbtree_t *tree, int dir)
{
    void *result = NULL;
    if (iter) {
        iter->tree = tree;
        iter->node = tree->root;
        iter->top  = 0;

        // Save the path for later selfersal
        if (iter->node != NULL) {
            while (iter->node->link[dir] != NULL) {
                iter->path[iter->top++] = iter->node;
                iter->node              = iter->node->link[dir];
            }
        }

        result = iter->node == NULL ? NULL : iter->node->value;
    }
    return result;
}

// Traverse a red black tree in the user-specified direction (0 asc, 1 desc)
static void *rbtree_iter_move(rbtree_iter_t *iter, int dir)
{
    if (iter->node->link[dir] != NULL) {
        // Continue down this branch
        iter->path[iter->top++] = iter->node;
        iter->node              = iter->node->link[dir];
        while (iter->node->link[!dir] != NULL) {
            iter->path[iter->top++] = iter->node;
            iter->node              = iter->node->link[!dir];
        }
    } else {
        // Move to the next branch
        rbtree_node_t *last = NULL;
        do {
            if (iter->top == 0) {
                iter->node = NULL;
                break;
            }
            last       = iter->node;
            iter->node = iter->path[--iter->top];
        } while (last == iter->node->link[dir]);
    }
    return iter->node == NULL ? NULL : iter->node->value;
}

void *rbtree_iter_first(rbtree_iter_t *iter, rbtree_t *tree)
{
    return rbtree_iter_start(iter, tree, 0);
}

void *rbtree_iter_last(rbtree_iter_t *iter, rbtree_t *tree)
{
    return rbtree_iter_start(iter, tree, 1);
}

void *rbtree_iter_next(rbtree_iter_t *iter)
{
    return rbtree_iter_move(iter, 1);
}

void *rbtree_iter_prev(rbtree_iter_t *iter)
{
    return rbtree_iter_move(iter, 0);
}

int rbtree_tree_test(rbtree_t *tree, rbtree_node_t *root)
{
    int lh, rh;

    if (root == NULL)
        return 1;
    else {
        rbtree_node_t *ln = root->link[0];
        rbtree_node_t *rn = root->link[1];

        /* Consecutive red links */
        if (rbtree_node_is_red(root)) {
            if (rbtree_node_is_red(ln) || rbtree_node_is_red(rn)) {
                pr_err("Red violation");
                return 0;
            }
        }

        lh = rbtree_tree_test(tree, ln);
        rh = rbtree_tree_test(tree, rn);

        /* Invalid binary search tree */
        if ((ln != NULL && tree->cmp(tree, ln, root) >= 0) || (rn != NULL && tree->cmp(tree, rn, root) <= 0)) {
            pr_err("Binary tree violation");
            return 0;
        }

        /* Black height mismatch */
        if (lh != 0 && rh != 0 && lh != rh) {
            pr_err("Black violation");
            return 0;
        }

        /* Only count black links */
        if (lh != 0 && rh != 0)
            return rbtree_node_is_red(root) ? lh : lh + 1;
        else
            return 0;
    }
}

static void rbtree_tree_print_iter(rbtree_t *tree,
                                   rbtree_node_t *node,
                                   rbtree_tree_node_f fun)
{
    assert(tree);
    assert(node);
    assert(fun);
    fun(tree, node);
    if (node->link[0])
        rbtree_tree_print_iter(tree, node->link[0], fun);
    if (node->link[1])
        rbtree_tree_print_iter(tree, node->link[1], fun);
}

void rbtree_tree_print(rbtree_t *tree, rbtree_tree_node_f fun)
{
    assert(tree);
    assert(fun);
    rbtree_tree_print_iter(tree, tree->root, fun);
}
