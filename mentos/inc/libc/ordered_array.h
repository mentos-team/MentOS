///                MentOS, The Mentoring Operating system project
/// @file ordered_array.h
/// @brief Interface for creating, inserting and deleting from ordered arrays.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// @brief This array is insertion sorted - it always remains in a sorted
///        state (between calls). It can store anything that can be cast to a
///        void* -- so a uint32_t, or any pointer.
typedef void *array_type_t;

/// @brief A predicate should return nonzero if the first argument is less
///        than the second. Else it should return zero.
typedef int8_t (*lessthan_predicate_t)(array_type_t, array_type_t);

/// @brief Structure which holds information concerning an ordered array.
typedef struct ordered_array_t {
	/// Pointer to the array.
	array_type_t *array;
	/// The size of the array.
	uint32_t size;
	/// The maximum size of the array.
	uint32_t max_size;
	/// Ordering fucntion.
	lessthan_predicate_t less_than;
} ordered_array_t;

/// @brief A standard less than predicate.
int8_t standard_lessthan_predicate(array_type_t a, array_type_t b);

/// @brief Create an ordered array.
ordered_array_t create_ordered_array(uint32_t max_size,
									 lessthan_predicate_t less_than);

/// @brief Set the ordered array.
ordered_array_t place_ordered_array(void *addr, uint32_t max_size,
									lessthan_predicate_t less_than);

/// @brief Destroy an ordered array.
void destroy_ordered_array(ordered_array_t *array);

/// @brief Add an item into the array.
void insert_ordered_array(array_type_t item, ordered_array_t *array);

/// @brief Lookup the item at index i.
array_type_t lookup_ordered_array(uint32_t i, ordered_array_t *array);

/// @brief Deletes the item at location i from the array.
void remove_ordered_array(uint32_t i, ordered_array_t *array);
