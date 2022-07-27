/// @file hashmap.h
/// @brief Functions for managing a structure that can map keys to values.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "klib/list.h"

// == OPAQUE TYPES ============================================================
/// @brief Stores information of an entry of the hashmap.
typedef struct hashmap_entry_t hashmap_entry_t;
/// @brief Stores information of a hashmap.
typedef struct hashmap_t hashmap_t;

// == HASHMAP FUNCTIONS =======================================================
/// @brief Hashing function, used to generate hash keys.
typedef unsigned int (*hashmap_hash_t)(const void *key);
/// @brief Comparison function, used to compare hash keys.
typedef int (*hashmap_comp_t)(const void *a, const void *b);
/// @brief Key duplication function, used to duplicate hash keys.
typedef void *(*hashmap_dupe_t)(const void *);
/// @brief Key deallocation function, used to free the memory occupied by hash keys.
typedef void (*hashmap_free_t)(void *);

// == HASHMAP KEY MANAGEMENT FUNCTIONS ========================================
/// @brief Transforms an integer key into a hash key.
/// @param key The integer key.
/// @return The resulting hash key.
unsigned int hashmap_int_hash(const void *key);

/// @brief Compares two integer hash keys.
/// @param a The first hash key.
/// @param b The second hash key.
/// @return Result of the comparison.
int hashmap_int_comp(const void *a, const void *b);

/// @brief Transforms a string key into a hash key.
/// @param key The string key.
/// @return The resulting hash key.
unsigned int hashmap_str_hash(const void *key);

/// @brief Compares two string hash keys.
/// @param a The first hash key.
/// @param b The second hash key.
/// @return Result of the comparison.
int hashmap_str_comp(const void *a, const void *b);

/// @brief This function can be passed as hashmap_dupe_t, it does nothing.
/// @param value The value to duplicate.
/// @return The duplicated value.
void *hashmap_do_not_duplicate(const void *value);

/// @brief This function can be passed as hashmap_free_t, it does nothing.
/// @param value The value to free.
void hashmap_do_not_free(void *value);

// == HASHMAP CREATION AND DESTRUCTION ========================================
/// @brief User-defined hashmap.
/// @param size Dimension of the hashmap.
/// @param hash_fun     The hashing function.
/// @param comp_fun     The hash compare function.
/// @param dupe_fun     The key duplication function.
/// @param key_free_fun The function used to free memory of keys.
/// @return A pointer to the hashmap.
/// @details
///     (key_free_fun) : No free function.
///     (val_free_fun) : Standard `free` function.
hashmap_t *hashmap_create(
    unsigned int size,
    hashmap_hash_t hash_fun,
    hashmap_comp_t comp_fun,
    hashmap_dupe_t dupe_fun,
    hashmap_free_t key_free_fun);

/// @brief Standard hashmap with keys of type (char *).
/// @param size Dimension of the hashmap.
/// @return A pointer to the hashmap.
/// @details
///     (key_free_fun) : Standard `free` function.
///     (val_free_fun) : Standard `free` function.
hashmap_t *hashmap_create_str(unsigned int size);

/// @brief Standard hashmap with keys of type (char *).
/// @param size Dimension of the hashmap.
/// @return A pointer to the hashmap.
/// @details
///     (key_free_fun) : No free function.
///     (val_free_fun) : Standard `free` function.
hashmap_t *hashmap_create_int(unsigned int size);

/// @brief Frees the memory of the hashmap.
/// @param map A pointer to the hashmap.
void hashmap_free(hashmap_t *map);

// == HASHMAP ACCESS FUNCTIONS ================================================
/// @brief Sets the `value` for the given `key` in the hashmap `map`.
/// @param map   The hashmap.
/// @param key   The entry key.
/// @param value The entry value.
/// @return NULL on success, a pointer to an already existing entry if fails.
void *hashmap_set(hashmap_t *map, const void *key, void *value);

/// @brief Access the value for the given key.
/// @param map The hashmap.
/// @param key The key of the entry we are searching.
/// @return The value on success, or NULL on failure.
void *hashmap_get(hashmap_t *map, const void *key);

/// @brief Removes the entry with the given key.
/// @param map The hashmap.
/// @param key The key of the entry we are searching.
/// @return The value on success, or NULL on failure.
void *hashmap_remove(hashmap_t *map, const void *key);

/// @brief Checks if the hashmap is empty.
/// @param map The hashmap.
/// @return 1 if empty, 0 otherwise.
int hashmap_is_empty(hashmap_t *map);

/// @brief Checks if the hashmap contains an entry with the given key.
/// @param map The hashmap.
/// @param key The key of the entry we are searching.
/// @return 1 if the entry is present, 0 otherwise.
int hashmap_has(hashmap_t *map, const void *key);

/// @brief Provides access to all the keys.
/// @param map The hashmap.
/// @return A list with all the keys, remember to destroy the list.
list_t *hashmap_keys(hashmap_t *map);

/// @brief Provides access to all the values.
/// @param map The hashmap.
/// @return A list with all the values, remember to destroy the list.
list_t *hashmap_values(hashmap_t *map);
