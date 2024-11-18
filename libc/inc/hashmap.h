/// @file hashmap.h
/// @brief Header file for a hashmap implementation with `char *` keys.

#pragma once

#include <stddef.h>

// Define the size of the hashmap
#define HASHMAP_SIZE 1024

/// @brief Structure representing a hashmap entry.
typedef struct hashmap_entry {
    /// The precomputed hash of the key.
    size_t hash;
    /// The value associated with the key.
    void *value;
    /// Pointer to the next entry (for handling collisions).
    struct hashmap_entry *next;
} hashmap_entry_t;

/// @brief Structure representing the hashmap.
typedef struct {
    /// Array of linked lists for separate chaining.
    hashmap_entry_t *buckets[HASHMAP_SIZE];
    /// Function to allocate an entry.
    hashmap_entry_t *(*alloc_entry)(void);
    /// Function to deallocate an entry.
    void (*dealloc_entry)(hashmap_entry_t *);
} hashmap_t;

/// @brief Hash function for generating an index from a key.
/// @param key The key to hash.
/// @return The hash index for the key.
size_t hash(const char *key);

/// @brief Initializes the hashmap with custom alloc and dealloc functions for
/// entries.
/// @param map Pointer to the hashmap to initialize.
/// @param alloc_fn Function to allocate entries, or NULL to use the default.
/// @param dealloc_fn Function to deallocate entries, or NULL to use the
/// default.
void hashmap_init(hashmap_t *map, hashmap_entry_t *(*alloc_fn)(void), void (*dealloc_fn)(hashmap_entry_t *));

/// @brief Inserts a key-value pair into the hashmap.
/// @param map Pointer to the hashmap.
/// @param key The key for the value.
/// @param value The value to store associated with the key.
void hashmap_insert(hashmap_t *map, const char *key, void *value);

/// @brief Retrieves the value associated with a given key.
/// @param map Pointer to the hashmap.
/// @param key The key to search for.
/// @return The value associated with the key, or NULL if the key is not found.
void *hashmap_get(hashmap_t *map, const char *key);

/// @brief Removes a key-value pair from the hashmap.
/// @param map Pointer to the hashmap.
/// @param key The key to remove.
void hashmap_remove(hashmap_t *map, const char *key);

/// @brief Destroys the hashmap and frees all allocated memory.
/// @param map Pointer to the hashmap to destroy.
void hashmap_destroy(hashmap_t *map);
