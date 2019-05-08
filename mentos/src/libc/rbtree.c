///                MentOS, The Mentoring Operating system project
/// @file rbtree.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "rbtree.h"
#include "assert.h"
#include "stdlib.h"

struct rbtree_node_t {
	/// Color red (1), black (0)
	int red;
	/// Link left [0] and right [1]
	rbtree_node_t *link[2];
	/// User provided, used indirectly via rbtree_tree_node_cmp_f.
	void *value;
};

struct rbtree_t {
	rbtree_node_t *root;
	rbtree_tree_node_cmp_f cmp;
	size_t size;
};

struct rbtree_iter_t {
	/// Pointer to the tree itself.
	rbtree_t *tree;
	/// Current node
	rbtree_node_t *node;
	/// Traversal path
	rbtree_node_t *path[RBTREE_ITER_MAX_HEIGHT];
	/// Top of stack
	size_t top;
};

rbtree_node_t *rbtree_node_alloc()
{
	return malloc(sizeof(rbtree_node_t));
}

rbtree_node_t *rbtree_node_init(rbtree_node_t *self, void *value)
{
	if (self) {
		self->red = 1;
		self->link[0] = self->link[1] = NULL;
		self->value = value;
	}

	return self;
}

rbtree_node_t *rbtree_node_create(void *value)
{
	return rbtree_node_init(rbtree_node_alloc(), value);
}

void *rbtree_node_get_value(rbtree_node_t *self)
{
	if (self) {
		return self->value;
	}

	return NULL;
}

void rbtree_node_dealloc(rbtree_node_t *self)
{
	if (self) {
		free(self);
	}
}

static int rbtree_node_is_red(const rbtree_node_t *self)
{
	return self ? self->red : 0;
}

static rbtree_node_t *rbtree_node_rotate(rbtree_node_t *self, int dir)
{
	rbtree_node_t *result = NULL;
	if (self) {
		result = self->link[!dir];
		self->link[!dir] = result->link[dir];
		result->link[dir] = self;
		self->red = 1;
		result->red = 0;
	}

	return result;
}

static rbtree_node_t *rbtree_node_rotate2(rbtree_node_t *self, int dir)
{
	rbtree_node_t *result = NULL;
	if (self) {
		self->link[!dir] = rbtree_node_rotate(self->link[!dir], !dir);
		result = rbtree_node_rotate(self, dir);
	}

	return result;
}

// rbtree_t - default callbacks.

static int rbtree_tree_node_cmp_ptr_cb(rbtree_t *self, rbtree_node_t *a,
				       rbtree_node_t *b)
{
	(void)self;

	return (a->value > b->value) - (a->value < b->value);
}

static void rbtree_tree_node_dealloc_cb(rbtree_t *self, rbtree_node_t *node)
{
	if (self) {
		if (node) {
			rbtree_node_dealloc(node);
		}
	}
}

// rbtree_t

rbtree_t *rbtree_tree_alloc()
{
	return malloc(sizeof(rbtree_t));
}

rbtree_t *rbtree_tree_init(rbtree_t *self, rbtree_tree_node_cmp_f node_cmp_cb)
{
	if (self) {
		self->root = NULL;
		self->size = 0;
		self->cmp =
			node_cmp_cb ? node_cmp_cb : rbtree_tree_node_cmp_ptr_cb;
	}

	return self;
}

rbtree_t *rbtree_tree_create(rbtree_tree_node_cmp_f node_cb)
{
	return rbtree_tree_init(rbtree_tree_alloc(), node_cb);
}

void rbtree_tree_dealloc(rbtree_t *self, rbtree_tree_node_f node_cb)
{
	assert(self);
	if (node_cb) {
		rbtree_node_t *node = self->root;
		rbtree_node_t *save = NULL;

		/* Rotate away the left links so that
         * we can treat this like the destruction
         * of a linked list.
         */
		while (node) {
			if (node->link[0] == NULL) {
				// No left links, just kill the node and move on.
				save = node->link[1];
				node_cb(self, node);
				free(node);
				node = NULL;
			} else {
				// Rotate away the left link and check again.
				save = node->link[0];
				node->link[0] = save->link[1];
				save->link[1] = node;
			}
			node = save;
		}
	}

	free(self);
}

void *rbtree_tree_find(rbtree_t *self, void *value)
{
	void *result = NULL;
	if (self) {
		rbtree_node_t node = { .value = value };
		rbtree_node_t *it = self->root;
		int cmp = 0;
		while (it) {
			if ((cmp = self->cmp(self, it, &node))) {
				/* If the tree supports duplicates, they should be
                 * chained to the right subtree for this to work.
                 */
				it = it->link[cmp < 0];
			} else {
				break;
			}
		}
		result = it ? it->value : NULL;
	}

	return result;
}

void *rbtree_tree_find_by_value(rbtree_t *self, rbtree_tree_cmp_f cmp_fun,
				void *value)
{
	void *result = NULL;
	if (self) {
		rbtree_node_t *it = self->root;
		int cmp = 0;
		while (it) {
			if ((cmp = cmp_fun(self, it, value))) {
				/* If the tree supports duplicates, they should be
                 * chained to the right subtree for this to work.
                 */
				it = it->link[cmp < 0];
			} else {
				break;
			}
		}
		result = it ? it->value : NULL;
	}

	return result;
}

// Creates (malloc'ates).
int rbtree_tree_insert(rbtree_t *self, void *value)
{
	return rbtree_tree_insert_node(self, rbtree_node_create(value));
}

// Returns 1 on success, 0 otherwise.
int rbtree_tree_insert_node(rbtree_t *self, rbtree_node_t *node)
{
	if (self && node) {
		if (self->root == NULL) {
			self->root = node;
		} else {
			// False tree root.
			rbtree_node_t head = { 0 };
			// Grandparent & parent.
			rbtree_node_t *g, *t;
			// Iterator & parent.
			rbtree_node_t *p, *q;
			int dir = 0, last = 0;

			// Set up our helpers.
			t = &head;
			g = p = NULL;
			q = t->link[1] = self->root;

			// Search down the tree for a place to insert.
			while (1) {
				if (q == NULL) {
					// Insert node at the first null link.
					p->link[dir] = q = node;
				} else if (rbtree_node_is_red(q->link[0]) &&
					   rbtree_node_is_red(q->link[1])) {
					// Simple red violation: color flip.
					q->red = 1;
					q->link[0]->red = 0;
					q->link[1]->red = 0;
				}

				if (rbtree_node_is_red(q) &&
				    rbtree_node_is_red(p)) {
					// Hard red violation: rotations necessary.
					int dir2 = t->link[1] == g;

					if (q == p->link[last]) {
						t->link[dir2] =
							rbtree_node_rotate(
								g, !last);
					} else {
						t->link[dir2] =
							rbtree_node_rotate2(
								g, !last);
					}
				}
				/* Stop working if we inserted a node. This
                 * check also disallows duplicates in the tree.
                 */
				if (self->cmp(self, q, node) == 0) {
					break;
				}
				last = dir;
				dir = self->cmp(self, q, node) < 0;

				// Move the helpers down.
				if (g != NULL) {
					t = g;
				}
				g = p, p = q;
				q = q->link[dir];
			}
			// Update the root (it may be different).
			self->root = head.link[1];
		}
		// Make the root black for simplified logic.
		self->root->red = 0;
		++self->size;
	}

	return 1;
}

/* Returns 1 if the value was removed, 0 otherwise. Optional node callback
 * can be provided to dealloc node and/or user data. Use
 * rbtree_tree_node_dealloc
 * default callback to deallocate node created by rbtree_tree_insert(...).
 */
int rbtree_tree_remove_with_cb(rbtree_t *self, void *value,
			       rbtree_tree_node_f node_cb)
{
	if (self->root != NULL) {
		// False tree root.
		rbtree_node_t head = { 0 };
		// Value wrapper node.
		rbtree_node_t node = { .value = value };
		// Helpers.
		rbtree_node_t *q, *p, *g;
		// Found item.
		rbtree_node_t *f = NULL;
		int dir = 1;

		// Set up our helpers.
		q = &head;
		g = p = NULL;
		q->link[1] = self->root;

		/* Search and push a red node down
         * to fix red violations as we go.
         */
		while (q->link[dir] != NULL) {
			int last = dir;

			// Move the helpers down.
			g = p, p = q;
			q = q->link[dir];
			dir = self->cmp(self, q, &node) < 0;

			/* Save the node with matching value and keep
             * going; we'll do removal tasks at the end.
             */
			if (self->cmp(self, q, &node) == 0) {
				f = q;
			}

			// Push the red node down with rotations and color flips.
			if (!rbtree_node_is_red(q) &&
			    !rbtree_node_is_red(q->link[dir])) {
				if (rbtree_node_is_red(q->link[!dir])) {
					p = p->link[last] =
						rbtree_node_rotate(q, dir);
				} else if (!rbtree_node_is_red(q->link[!dir])) {
					rbtree_node_t *s = p->link[!last];
					if (s) {
						if (!rbtree_node_is_red(
							    s->link[!last]) &&
						    !rbtree_node_is_red(
							    s->link[last])) {
							// Color flip.
							p->red = 0;
							s->red = 1;
							q->red = 1;
						} else {
							int dir2 =
								g->link[1] == p;
							if (rbtree_node_is_red(
								    s->link[last])) {
								g->link[dir2] = rbtree_node_rotate2(
									p,
									last);
							} else if (
								rbtree_node_is_red(
									s->link[!last])) {
								g->link[dir2] = rbtree_node_rotate(
									p,
									last);
							}
							// Ensure correct coloring.
							q->red = g->link[dir2]
									 ->red =
								1;
							g->link[dir2]
								->link[0]
								->red = 0;
							g->link[dir2]
								->link[1]
								->red = 0;
						}
					}
				}
			}
		}
		// Replace and remove the saved node.
		if (f) {
			void *tmp = f->value;
			f->value = q->value;
			q->value = tmp;

			p->link[p->link[1] == q] = q->link[q->link[0] == NULL];

			if (node_cb) {
				node_cb(self, q);
			}
			q = NULL;
		}

		// Update the root (it may be different).
		self->root = head.link[1];

		// Make the root black for simplified logic.
		if (self->root != NULL) {
			self->root->red = 0;
		}

		--self->size;
	}

	return 1;
}

int rbtree_tree_remove(rbtree_t *self, void *value)
{
	int result = 0;
	if (self) {
		result = rbtree_tree_remove_with_cb(
			self, value, rbtree_tree_node_dealloc_cb);
	}

	return result;
}

size_t rbtree_tree_size(rbtree_t *self)
{
	size_t result = 0;
	if (self) {
		result = self->size;
	}

	return result;
}

// rbtree_iter_t

rbtree_iter_t *rbtree_iter_alloc()
{
	return malloc(sizeof(rbtree_iter_t));
}

rbtree_iter_t *rbtree_iter_init(rbtree_iter_t *self)
{
	if (self) {
		self->tree = NULL;
		self->node = NULL;
		self->top = 0;
	}

	return self;
}

rbtree_iter_t *rbtree_iter_create()
{
	return rbtree_iter_init(rbtree_iter_alloc());
}

void rbtree_iter_dealloc(rbtree_iter_t *self)
{
	if (self) {
		free(self);
	}
}

/* Internal function, init traversal object, dir determines whether
 * to begin traversal at the smallest or largest valued node.
 */
static void *rbtree_iter_start(rbtree_iter_t *self, rbtree_t *tree, int dir)
{
	void *result = NULL;
	if (self) {
		self->tree = tree;
		self->node = tree->root;
		self->top = 0;
		// Save the path for later selfersal.
		if (self->node != NULL) {
			while (self->node->link[dir] != NULL) {
				self->path[self->top++] = self->node;
				self->node = self->node->link[dir];
			}
		}

		result = self->node == NULL ? NULL : self->node->value;
	}

	return result;
}

// Traverse a red black tree in the user-specified direction (0 asc, 1 desc).
static void *rbtree_iter_move(rbtree_iter_t *self, int dir)
{
	if (self->node->link[dir] != NULL) {
		// Continue down this branch.
		self->path[self->top++] = self->node;
		self->node = self->node->link[dir];
		while (self->node->link[!dir] != NULL) {
			self->path[self->top++] = self->node;
			self->node = self->node->link[!dir];
		}
	} else {
		// Move to the next branch.
		rbtree_node_t *last = NULL;
		do {
			if (self->top == 0) {
				self->node = NULL;

				break;
			}
			last = self->node;
			self->node = self->path[--self->top];
		} while (last == self->node->link[dir]);
	}

	return self->node == NULL ? NULL : self->node->value;
}

void *rbtree_iter_first(rbtree_iter_t *self, rbtree_t *tree)
{
	return rbtree_iter_start(self, tree, 0);
}

void *rbtree_iter_last(rbtree_iter_t *self, rbtree_t *tree)
{
	return rbtree_iter_start(self, tree, 1);
}

void *rbtree_iter_next(rbtree_iter_t *self)
{
	return rbtree_iter_move(self, 1);
}

void *rbtree_iter_prev(rbtree_iter_t *self)
{
	return rbtree_iter_move(self, 0);
}

int rbtree_tree_test(rbtree_t *self, rbtree_node_t *root)
{
	int lh, rh;

	if (root == NULL) {
		return 1;
	} else {
		rbtree_node_t *ln = root->link[0];
		rbtree_node_t *rn = root->link[1];

		// Consecutive red links.
		if (rbtree_node_is_red(root)) {
			if (rbtree_node_is_red(ln) || rbtree_node_is_red(rn)) {
				printf("Red violation");

				return 0;
			}
		}

		lh = rbtree_tree_test(self, ln);
		rh = rbtree_tree_test(self, rn);

		// Invalid binary search tree.
		if ((ln != NULL && self->cmp(self, ln, root) >= 0) ||
		    (rn != NULL && self->cmp(self, rn, root) <= 0)) {
			puts("Binary tree violation");

			return 0;
		}

		// Black height mismatch.
		if (lh != 0 && rh != 0 && lh != rh) {
			puts("Black violation");

			return 0;
		}

		// Only count black links.
		if (lh != 0 && rh != 0) {
			return rbtree_node_is_red(root) ? lh : lh + 1;
		} else {
			return 0;
		}
	}
}

static void rbtree_tree_print_iter(rbtree_t *self, rbtree_node_t *node,
				   rbtree_tree_node_f fun)
{
	assert(self);
	assert(node);
	assert(fun);

	fun(self, node);
	if (node->link[0]) {
		rbtree_tree_print_iter(self, node->link[0], fun);
	}
	if (node->link[1]) {
		rbtree_tree_print_iter(self, node->link[1], fun);
	}
}

void rbtree_tree_print(rbtree_t *self, rbtree_tree_node_f fun)
{
	assert(self);
	assert(fun);

	rbtree_tree_print_iter(self, self->root, fun);
}
