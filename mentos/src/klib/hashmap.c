/// @file hashmap.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "klib/hashmap.h"
#include "assert.h"
#include "string.h"
#include "mem/slab.h"

/// @brief Stores information of an entry of the hashmap.
struct hashmap_entry_t {
    /// Key of the entry.
    char *key;
    /// Value of the entry.
    void *value;
    /// Pointer to the next entry.
    struct hashmap_entry_t *next;
};

/// @brief Stores information of a hashmap.
struct hashmap_t {
    /// Hashing function, used to generate hash keys.
    hashmap_hash_t hash_func;
    /// Comparison function, used to compare hash keys.
    hashmap_comp_t hash_comp;
    /// Key duplication function, used to duplicate hash keys.
    hashmap_dupe_t hash_key_dup;
    /// Key deallocation function, used to free the memory occupied by hash keys.
    hashmap_free_t hash_key_free;
    /// Size of the hashmap.
    unsigned int size;
    /// List of entries.
    hashmap_entry_t **entries;
};

static inline hashmap_t *__alloc_hashmap()
{
    hashmap_t *hashmap = kmalloc(sizeof(hashmap_t));
    memset(hashmap, 0, sizeof(hashmap_t));
    return hashmap;
}

static inline hashmap_entry_t *__alloc_entry()
{
    hashmap_entry_t *entry = kmalloc(sizeof(hashmap_entry_t));
    memset(entry, 0, sizeof(hashmap_entry_t));
    return entry;
}

static inline void __dealloc_entry(hashmap_entry_t *entry)
{
    assert(entry && "Invalid pointer to an entry.");
    kfree(entry);
}

static inline hashmap_entry_t **__alloc_entries(unsigned int size)
{
    hashmap_entry_t **entries = kmalloc(sizeof(hashmap_entry_t *) * size);
    memset(entries, 0, sizeof(hashmap_entry_t *) * size);
    return entries;
}

static inline void __dealloc_entries(hashmap_entry_t **entries)
{
    assert(entries && "Invalid pointer to entries.");
    kfree(entries);
}

unsigned int hashmap_int_hash(const void *key)
{
    return (unsigned int)key;
}

int hashmap_int_comp(const void *a, const void *b)
{
    return (int)a == (int)b;
}

unsigned int hashmap_str_hash(const void *_key)
{
    unsigned int hash = 0;
    const char *key   = (const char *)_key;
    char c;
    // This is the so-called "sdbm" hash. It comes from a piece of public
    // domain code from a clone of ndbm.
    while ((c = *key++))
        hash = c + (hash << 6) + (hash << 16) - hash;
    return hash;
}

int hashmap_str_comp(const void *a, const void *b)
{
    return !strcmp(a, b);
}

void *hashmap_do_not_duplicate(const void *value)
{
    return (void *)value;
}

void hashmap_do_not_free(void *value)
{
    (void)value;
}

hashmap_t *hashmap_create(
    unsigned int size,
    hashmap_hash_t hash_fun,
    hashmap_comp_t comp_fun,
    hashmap_dupe_t dupe_fun,
    hashmap_free_t key_free_fun)
{
    // Allocate the map.
    hashmap_t *map = __alloc_hashmap();
    // Initialize the entries.
    map->size    = size;
    map->entries = __alloc_entries(size);
    // Initialize its functions.
    map->hash_func     = hash_fun;
    map->hash_comp     = comp_fun;
    map->hash_key_dup  = dupe_fun;
    map->hash_key_free = key_free_fun;
    return map;
}

void hashmap_free(hashmap_t *map)
{
    for (unsigned int i = 0; i < map->size; ++i) {
        hashmap_entry_t *x = map->entries[i], *p;
        while (x) {
            p = x;
            x = x->next;
            map->hash_key_free(p->key);
            __dealloc_entry(p);
        }
    }
    __dealloc_entries(map->entries);
}

void *hashmap_set(hashmap_t *map, const void *key, void *value)
{
    unsigned int hash  = map->hash_func(key) % map->size;
    hashmap_entry_t *x = map->entries[hash];

    if (x == NULL) {
        hashmap_entry_t *e = __alloc_entry();
        e->key             = map->hash_key_dup(key);
        e->value           = value;
        e->next            = NULL;
        map->entries[hash] = e;

        return NULL;
    }

    hashmap_entry_t *p = NULL;

    do {
        if (map->hash_comp(x->key, key)) {
            void *out = x->value;
            x->value  = value;

            return out;
        }
        p = x;
        x = x->next;
    } while (x);

    hashmap_entry_t *e = __alloc_entry();
    e->key             = map->hash_key_dup(key);
    e->value           = value;
    e->next            = NULL;
    p->next            = e;

    return NULL;
}

void *hashmap_get(hashmap_t *map, const void *key)
{
    unsigned int hash = map->hash_func(key) % map->size;
    for (hashmap_entry_t *x = map->entries[hash]; x; x = x->next)
        if (map->hash_comp(x->key, key))
            return x->value;
    return NULL;
}

void *hashmap_remove(hashmap_t *map, const void *key)
{
    unsigned int hash  = map->hash_func(key) % map->size;
    hashmap_entry_t *x = map->entries[hash];

    if (x == NULL) {
        return NULL;
    }
    if (map->hash_comp(x->key, key)) {
        void *out          = x->value;
        map->entries[hash] = x->next;
        map->hash_key_free(x->key);
        __dealloc_entry(x);
        return out;
    }

    hashmap_entry_t *p = x;
    x                  = x->next;
    do {
        if (map->hash_comp(x->key, key)) {
            void *out = x->value;
            p->next   = x->next;
            map->hash_key_free(x->key);
            __dealloc_entry(x);
            return out;
        }
        p = x;
        x = x->next;
    } while (x);

    return NULL;
}

int hashmap_is_empty(hashmap_t *map)
{
    for (unsigned int i = 0; i < map->size; ++i)
        if (map->entries[i])
            return 0;
    return 1;
}

int hashmap_has(hashmap_t *map, const void *key)
{
    unsigned int hash = map->hash_func(key) % map->size;
    for (hashmap_entry_t *x = map->entries[hash]; x; x = x->next)
        if (map->hash_comp(x->key, key))
            return 1;
    return 0;
}

list_t *hashmap_keys(hashmap_t *map)
{
    list_t *l = list_create();
    for (unsigned int i = 0; i < map->size; ++i)
        for (hashmap_entry_t *x = map->entries[i]; x; x = x->next)
            list_insert_back(l, x->key);
    return l;
}

list_t *hashmap_values(hashmap_t *map)
{
    list_t *l = list_create();
    for (unsigned int i = 0; i < map->size; ++i)
        for (hashmap_entry_t *x = map->entries[i]; x; x = x->next)
            list_insert_back(l, x->value);
    return l;
}
