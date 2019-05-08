///                MentOS, The Mentoring Operating system project
/// @file rbtree.h
/// @brief Implementation of red black tree data structure.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

#ifndef RBTREE_ITER_MAX_HEIGHT

/// Tallest allowable tree to iterate.
#define RBTREE_ITER_MAX_HEIGHT 64
#endif

//==============================================================================
// Opaque types.

// TODO: doxygen comment.
typedef struct rbtree_t rbtree_t;

// TODO: doxygen comment.
typedef struct rbtree_node_t rbtree_node_t;

// TODO: doxygen comment.
typedef struct rbtree_iter_t rbtree_iter_t;

//==============================================================================
// Comparison functions.

// TODO: doxygen comment.
typedef int (*rbtree_tree_cmp_f)(rbtree_t *self, rbtree_node_t *a, void *arg);

// TODO: doxygen comment.
typedef int (*rbtree_tree_node_cmp_f)(rbtree_t *self, rbtree_node_t *a,
				      rbtree_node_t *b);

// TODO: doxygen comment.
typedef void (*rbtree_tree_node_f)(rbtree_t *self, rbtree_node_t *node);

//==============================================================================
// Node management functions.

// TODO: doxygen comment.
rbtree_node_t *rbtree_node_alloc();

// TODO: doxygen comment.
rbtree_node_t *rbtree_node_create(void *value);

// TODO: doxygen comment.
rbtree_node_t *rbtree_node_init(rbtree_node_t *self, void *value);

// TODO: doxygen comment.
void *rbtree_node_get_value(rbtree_node_t *self);

// TODO: doxygen comment.
void rbtree_node_dealloc(rbtree_node_t *self);

//==============================================================================
// Tree management functions.

// TODO: doxygen comment.
rbtree_t *rbtree_tree_alloc();

// TODO: doxygen comment.
rbtree_t *rbtree_tree_create(rbtree_tree_node_cmp_f cmp);

// TODO: doxygen comment.
rbtree_t *rbtree_tree_init(rbtree_t *self, rbtree_tree_node_cmp_f cmp);

// TODO: doxygen comment.
void rbtree_tree_dealloc(rbtree_t *self, rbtree_tree_node_f node_cb);

// TODO: doxygen comment.
void *rbtree_tree_find(rbtree_t *self, void *value);

// TODO: doxygen comment.
void *rbtree_tree_find_by_value(rbtree_t *self, rbtree_tree_cmp_f cmp_fun,
				void *value);

// TODO: doxygen comment.
int rbtree_tree_insert(rbtree_t *self, void *value);

// TODO: doxygen comment.
int rbtree_tree_remove(rbtree_t *self, void *value);

// TODO: doxygen comment.
size_t rbtree_tree_size(rbtree_t *self);

// TODO: doxygen comment.
int rbtree_tree_insert_node(rbtree_t *self, rbtree_node_t *node);

// TODO: doxygen comment.
int rbtree_tree_remove_with_cb(rbtree_t *self, void *value,
			       rbtree_tree_node_f node_cb);

//==============================================================================
// Iterators.

// TODO: doxygen comment.
rbtree_iter_t *rbtree_iter_alloc();

// TODO: doxygen comment.
rbtree_iter_t *rbtree_iter_init(rbtree_iter_t *self);

// TODO: doxygen comment.
rbtree_iter_t *rbtree_iter_create();

// TODO: doxygen comment.
void rbtree_iter_dealloc(rbtree_iter_t *self);

// TODO: doxygen comment.
void *rbtree_iter_first(rbtree_iter_t *self, rbtree_t *tree);

// TODO: doxygen comment.
void *rbtree_iter_last(rbtree_iter_t *self, rbtree_t *tree);

// TODO: doxygen comment.
void *rbtree_iter_next(rbtree_iter_t *self);

// TODO: doxygen comment.
void *rbtree_iter_prev(rbtree_iter_t *self);

//==============================================================================
// Tree debugging functions.

// TODO: doxygen comment.
int rbtree_tree_test(rbtree_t *self, rbtree_node_t *root);

// TODO: doxygen comment.
void rbtree_tree_print(rbtree_t *self, rbtree_tree_node_f fun);

//==============================================================================
