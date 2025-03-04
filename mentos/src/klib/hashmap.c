/// @file hashmap.c
/// @brief Source file for a hashmap implementation with `char *` keys.

#include "hashmap.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

size_t hash(const char *key)
{
    size_t hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash % HASHMAP_SIZE;
}

void hashmap_init(hashmap_t *map, hashmap_entry_t *(*alloc_fn)(void), void (*dealloc_fn)(hashmap_entry_t *))
{
    assert(map && "Hashmap is NULL.");
    memset(map->buckets, 0, sizeof(map->buckets));
    map->alloc_entry   = alloc_fn;
    map->dealloc_entry = dealloc_fn;
}

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
