/// @file hashmap.c
/// @brief Source file for a hashmap implementation with `char *` keys.

#include "hashmap.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/// @brief Computes a hash index for a given key using the djb2 algorithm.
/// @param key The key to hash.
/// @return The hash index for the key.
size_t hash(const char *key)
{
    size_t hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash % HASHMAP_SIZE;
}

/// @brief Initializes the hashmap with custom alloc and dealloc functions for
/// entries. If `alloc_entry` or `dealloc_entry` is NULL, the default functions
/// are used.
/// @param map Pointer to the hashmap to initialize.
/// @param alloc_entry Function to allocate entries, or NULL to use the default.
/// @param dealloc_entry Function to deallocate entries, or NULL to use the
/// default.
void hashmap_init(hashmap_t *map, hashmap_entry_t *(*alloc_fn)(void), void (*dealloc_fn)(hashmap_entry_t *))
{
    assert(map && "Hashmap is NULL.");
    memset(map->buckets, 0, sizeof(map->buckets));
    map->alloc_entry   = alloc_fn;
    map->dealloc_entry = dealloc_fn;
}

/// @brief Inserts a key-value pair into the hashmap.
/// @param map Pointer to the hashmap.
/// @param key The key for the value.
/// @param value The value to store associated with the key.
void hashmap_insert(hashmap_t *map, const char *key, void *value)
{
    assert(map && "Hashmap is NULL.");
    assert(key && "Key is NULL.");

    size_t hashed_key          = hash(key);
    size_t index               = hashed_key % HASHMAP_SIZE;
    hashmap_entry_t *new_entry = map->alloc_entry();
    assert(new_entry && "Failed to allocate memory for hashmap entry.");

    new_entry->hash     = hashed_key;
    new_entry->value    = value;
    new_entry->next     = map->buckets[index];
    map->buckets[index] = new_entry;
}

/// @brief Retrieves the value associated with a given key.
/// @param map Pointer to the hashmap.
/// @param key The key to search for.
/// @return The value associated with the key, or NULL if the key is not found.
void *hashmap_get(hashmap_t *map, const char *key)
{
    assert(map && "Hashmap is NULL.");
    assert(key && "Key is NULL.");

    size_t hashed_key      = hash(key);
    size_t index           = hashed_key % HASHMAP_SIZE;
    hashmap_entry_t *entry = map->buckets[index];

    while (entry != NULL) {
        if (entry->hash == hashed_key) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL; // Key not found
}

/// @brief Removes a key-value pair from the hashmap.
/// @param map Pointer to the hashmap.
/// @param key The key to remove.
void hashmap_remove(hashmap_t *map, const char *key)
{
    assert(map && "Hashmap is NULL.");
    assert(key && "Key is NULL.");

    size_t hashed_key      = hash(key);
    size_t index           = hashed_key % HASHMAP_SIZE;
    hashmap_entry_t *entry = map->buckets[index];
    hashmap_entry_t *prev  = NULL;

    while (entry != NULL) {
        if (entry->hash == hashed_key) {
            if (prev == NULL) {
                map->buckets[index] = entry->next;
            } else {
                prev->next = entry->next;
            }
            map->dealloc_entry(entry);
            return;
        }
        prev  = entry;
        entry = entry->next;
    }
}

/// @brief Destroys the hashmap and frees all allocated memory.
/// @param map Pointer to the hashmap to destroy.
void hashmap_destroy(hashmap_t *map)
{
    assert(map && "Hashmap is NULL.");

    for (int i = 0; i < HASHMAP_SIZE; i++) {
        hashmap_entry_t *entry = map->buckets[i];
        while (entry != NULL) {
            hashmap_entry_t *next_entry = entry->next;
            map->dealloc_entry(entry);
            entry = next_entry;
        }
        map->buckets[i] = NULL;
    }
}
