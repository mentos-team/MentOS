///                MentOS, The Mentoring Operating system project
/// @file hashmap.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "list.h"
#include "stdbool.h"

//==============================================================================
// Opaque types.
typedef struct hashmap_entry_t hashmap_entry_t;
typedef struct hashmap_t hashmap_t;

//=============================================================================
// Hashmap functions.

// TODO: doxygen comment.
typedef size_t (*hashmap_hash_t)(void *key);

// TODO: doxygen comment.
typedef bool_t (*hashmap_comp_t)(void *a, void *b);

// TODO: doxygen comment.
typedef void (*hashmap_free_t)(void *);

// TODO: doxygen comment.
typedef void *(*hashmap_dupe_t)(void *);

//==============================================================================
// Hashmap creation and destruction.

// TODO: doxygen comment.
extern hashmap_t *hashmap_create(size_t size);

// TODO: doxygen comment.
extern hashmap_t *hashmap_create_int(size_t size);

// TODO: doxygen comment.
extern void hashmap_free(hashmap_t *map);

//==============================================================================
// Hashmap management.

// TODO: doxygen comment.
extern void *hashmap_set(hashmap_t *map, void *key, void *value);

// TODO: doxygen comment.
extern void *hashmap_get(hashmap_t *map, void *key);

// TODO: doxygen comment.
extern void *hashmap_remove(hashmap_t *map, void *key);

//==============================================================================
// Hashmap search.

// TODO: doxygen comment.
extern bool_t hashmap_is_empty(hashmap_t *map);

// TODO: doxygen comment.
extern bool_t hashmap_has(hashmap_t *map, void *key);

// TODO: doxygen comment.
extern list_t *hashmap_keys(hashmap_t *map);

// TODO: doxygen comment.
extern list_t *hashmap_values(hashmap_t *map);

// TODO: doxygen comment.
extern size_t hashmap_string_hash(void *key);

// TODO: doxygen comment.
extern bool_t hashmap_string_comp(void *a, void *b);

// TODO: doxygen comment.
extern void *hashmap_string_dupe(void *key);

//==============================================================================
