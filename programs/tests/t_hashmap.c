/// @file t_hashmap.c
/// @brief This program tests hashmaps.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hashmap.h"

// Custom allocation function for hashmap entries
hashmap_entry_t *alloc_entry(void)
{
    hashmap_entry_t *entry = (hashmap_entry_t *)malloc(sizeof(hashmap_entry_t));
    assert(entry && "Failed to allocate hashmap entry.");
    return entry;
}

// Custom deallocation function for hashmap entries
void dealloc_entry(hashmap_entry_t *entry)
{
    assert(entry && "Invalid entry pointer.");
    free(entry); // Free the entry itself
}

int main()
{
    hashmap_t map;
    hashmap_init(&map, alloc_entry, dealloc_entry);

    // Test inserting and retrieving values
    hashmap_insert(&map, "apple", "A sweet red fruit");
    hashmap_insert(&map, "banana", "A long yellow fruit");
    hashmap_insert(&map, "grape", "A small purple or green fruit");

    if (strcmp(hashmap_get(&map, "apple"), "A sweet red fruit") != 0) {
        fprintf(stderr, "Error: Failed to retrieve 'apple'\n");
        return 1;
    }
    if (strcmp(hashmap_get(&map, "banana"), "A long yellow fruit") != 0) {
        fprintf(stderr, "Error: Failed to retrieve 'banana'\n");
        return 1;
    }
    if (strcmp(hashmap_get(&map, "grape"), "A small purple or green fruit") != 0) {
        fprintf(stderr, "Error: Failed to retrieve 'grape'\n");
        return 1;
    }

    // Test retrieving a non-existent key
    if (hashmap_get(&map, "orange") != NULL) {
        fprintf(stderr, "Error: Retrieved value for non-existent key 'orange'\n");
        return 1;
    }

    // Test updating an existing key
    hashmap_insert(&map, "apple", "A popular fruit often red or green");
    if (strcmp(hashmap_get(&map, "apple"), "A popular fruit often red or green") != 0) {
        fprintf(stderr, "Error: Failed to update value for 'apple'\n");
        return 1;
    }

    // Test removing a key
    hashmap_remove(&map, "banana");
    if (hashmap_get(&map, "banana") != NULL) {
        fprintf(stderr, "Error: Key 'banana' was not removed\n");
        return 1;
    }

    // Test removing a non-existent key (should not cause any errors)
    hashmap_remove(&map, "pineapple");

    // Test reinserting and retrieval after some removals
    hashmap_insert(&map, "banana", "A reinserted long yellow fruit");
    if (strcmp(hashmap_get(&map, "banana"), "A reinserted long yellow fruit") != 0) {
        fprintf(stderr, "Error: Failed to retrieve reinserted 'banana'\n");
        return 1;
    }

    // Test the removal of all items and final cleanup
    hashmap_destroy(&map);
    if (hashmap_get(&map, "apple") != NULL) {
        fprintf(stderr, "Error: Key 'apple' still exists after destroy\n");
        return 1;
    }
    if (hashmap_get(&map, "grape") != NULL) {
        fprintf(stderr, "Error: Key 'grape' still exists after destroy\n");
        return 1;
    }
    if (hashmap_get(&map, "banana") != NULL) {
        fprintf(stderr, "Error: Key 'banana' still exists after destroy\n");
        return 1;
    }

    return 0;
}