///                MentOS, The Mentoring Operating system project
/// @file bitset.c
/// @brief Bitset data structure.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "bitset.h"
#include "math.h"
#include "string.h"
#include "stdlib.h"
#include "assert.h"

/// @brief
/// @param bit
/// @param index
/// @param mask
static void get_index_and_mask(size_t bit, size_t *index, size_t *mask)
{
	assert(index);

	assert(mask);

	(*index) = bit >> 3;

	bit = bit - (*index) * 8;

	size_t offset = bit & 7;

	(*mask) = 1UL << offset;
}

/// @brief
/// @param set
/// @param size

/*
 * static void bitset_resize(bitset_t *set, size_t size)
 * {
 *     assert(set && "Received NULL set.");
 *
 *     if (set->size >= size)
 *     {
 *         return;
 *     }
 *     set->data = realloc(set->data, size);
 *
 *     memset(set->data + set->size, 0, size - set->size);
 *
 *     set->size = size;
 *
 * }
 */

void bitset_init(bitset_t *set, size_t size)
{
	assert(set && "Received NULL set.");

	set->size = ceil(size, 8);

	set->data = calloc(set->size, 1);
}

void bitset_free(bitset_t *set)
{
	assert(set && "Received NULL set.");

	free(set->data);
}

/*
 * void bitset_set(bitset_t *set, size_t bit)
 * {
 *     assert(set && "Received NULL set.");
 *     size_t index, mask;
 *
 *     get_index_and_mask(bit, &index, &mask);
 *
 *      if (set->size <= index)
 *      {
 *          bitset_resize(set, set->size << 1);
 *      }
 *
 * set->data[index] |= mask;
 * }
 */

void bitset_clear(bitset_t *set, size_t bit)
{
	assert(set && "Received NULL set.");

	size_t index, mask;

	get_index_and_mask(bit, &index, &mask);

	set->data[index] &= ~mask;
}

bool_t bitset_test(bitset_t *set, size_t bit)
{
	assert(set && "Received NULL set.");

	size_t index, mask;

	get_index_and_mask(bit, &index, &mask);

	return (mask & set->data[index]) != 0;
}

signed long bitset_find_first_unset_bit(bitset_t *set)
{
	assert(set && "Received NULL set.");

	for (size_t i = 0; i < (set->size * 8); i++) {
		if (!bitset_test(set, i)) {
			return (signed long)i;
		}
	}

	return -1;
}
