///                MentOS, The Mentoring Operating system project
/// @file hashmap.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "hashmap.h"
#include "string.h"
#include "stdlib.h"

struct hashmap_entry_t
{
    char *key;
    void *value;
    struct hashmap_entry_t *next;
};

struct hashmap_t
{
    hashmap_hash_t hash_func;
    hashmap_comp_t hash_comp;
    hashmap_dupe_t hash_key_dup;
    hashmap_free_t hash_key_free;
    hashmap_free_t hash_val_free;
    size_t size;
    hashmap_entry_t **entries;
};

size_t hashmap_string_hash(void *_key)
{
    size_t hash = 0;
    char *key = (char *) _key;
    int c;
    /*
     * This is the so-called "sdbm" hash. It comes from a piece of public
     * domain code from a clone of ndbm.
     */
    while ((c = *key++))
    {
        hash = c + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

bool_t hashmap_string_comp(void *a, void *b)
{
    return !strcmp(a, b);
}

void *hashmap_string_dupe(void *key)
{
    return strdup(key);
}

static size_t hashmap_int_hash(void *key)
{
    return (size_t) key;
}

static bool_t hashmap_int_comp(void *a, void *b)
{
    return (int) a == (int) b;
}

static void *hashmap_int_dupe(void *key)
{
    return key;
}

static void hashmap_int_free(void *ptr)
{
    (void) ptr;
}

hashmap_t *hashmap_create(size_t size)
{
    hashmap_t *map = malloc(sizeof(hashmap_t));

    map->hash_func = &hashmap_string_hash;
    map->hash_comp = &hashmap_string_comp;
    map->hash_key_dup = &hashmap_string_dupe;
    map->hash_key_free = &free;
    map->hash_val_free = &free;

    map->size = size;
    map->entries = malloc(sizeof(hashmap_entry_t *) * size);
    memset(map->entries, 0, sizeof(hashmap_entry_t *) * size);

    return map;
}

hashmap_t *hashmap_create_int(size_t size)
{
    hashmap_t *map = malloc(sizeof(hashmap_t));

    map->hash_func = &hashmap_int_hash;
    map->hash_comp = &hashmap_int_comp;
    map->hash_key_dup = &hashmap_int_dupe;
    map->hash_key_free = &hashmap_int_free;
    map->hash_val_free = &free;

    map->size = size;
    map->entries = malloc(sizeof(hashmap_entry_t *) * size);
    memset(map->entries, 0, sizeof(hashmap_entry_t *) * size);

    return map;
}

void hashmap_free(hashmap_t *map)
{
    for (size_t i = 0; i < map->size; ++i)
    {
        hashmap_entry_t *x = map->entries[i], * p;
        while (x)
        {
            p = x;
            x = x->next;
            map->hash_key_free(p->key);
            map->hash_val_free(p);
        }
    }

    free(map->entries);
}

void *hashmap_set(hashmap_t *map, void *key, void *value)
{
    size_t hash = map->hash_func(key) % map->size;
    hashmap_entry_t *x = map->entries[hash];

    if (x == NULL)
    {
        hashmap_entry_t *e = malloc(sizeof(hashmap_entry_t));
        e->key = map->hash_key_dup(key);
        e->value = value;
        e->next = NULL;
        map->entries[hash] = e;

        return NULL;
    }

    hashmap_entry_t *p = NULL;

    do
    {
        if (map->hash_comp(x->key, key))
        {
            void *out = x->value;
            x->value = value;

            return out;
        }
        p = x;
        x = x->next;
    } while (x);

    hashmap_entry_t *e = malloc(sizeof(hashmap_entry_t));
    e->key = map->hash_key_dup(key);
    e->value = value;
    e->next = NULL;
    p->next = e;

    return NULL;
}

void *hashmap_get(hashmap_t *map, void *key)
{
    size_t hash = map->hash_func(key) % map->size;
    hashmap_entry_t *x = map->entries[hash];

    if (x == NULL)
    {
        return NULL;
    }
    do
    {
        if (map->hash_comp(x->key, key))
        {
            return x->value;
        }
        x = x->next;
    } while (x);

    return NULL;
}

void *hashmap_remove(hashmap_t *map, void *key)
{
    size_t hash = map->hash_func(key) % map->size;
    hashmap_entry_t *x = map->entries[hash];

    if (x == NULL)
    {
        return NULL;
    }
    if (map->hash_comp(x->key, key))
    {
        void *out = x->value;
        map->entries[hash] = x->next;
        map->hash_key_free(x->key);
        map->hash_val_free(x);

        return out;
    }

    hashmap_entry_t * p = x;
    x = x->next;
    do
    {
        if (map->hash_comp(x->key, key))
        {
            void *out = x->value;
            p->next = x->next;
            map->hash_key_free(x->key);
            map->hash_val_free(x);

            return out;
        }
        p = x;
        x = x->next;
    } while (x);

    return NULL;
}

bool_t hashmap_is_empty(hashmap_t *map)
{
    for (size_t i = 0; i < map->size; ++i)
    {
        if (map->entries[i])
        {
            return false;
        }
    }

    return true;
}

bool_t hashmap_has(hashmap_t *map, void *key)
{
    size_t hash = map->hash_func(key) % map->size;
    hashmap_entry_t * x = map->entries[hash];

    if (x == NULL)
    {
        return false;
    }
    do
    {
        if (map->hash_comp(x->key, key))
        {
            return true;
        }
        x = x->next;
    } while (x);

    return false;
}

list_t *hashmap_keys(hashmap_t *map)
{
    list_t *l = list_create();

    for (size_t i = 0; i < map->size; ++i)
    {
        hashmap_entry_t * x = map->entries[i];
        while (x)
        {
            list_insert_back(l, x->key);
            x = x->next;
        }
    }

    return l;
}

list_t *hashmap_values(hashmap_t *map)
{
    list_t *l = list_create();

    for (size_t i = 0; i < map->size; ++i)
    {
        hashmap_entry_t *x = map->entries[i];

        while (x)
        {
            list_insert_back(l, x->value);
            x = x->next;
        }
    }

    return l;
}
