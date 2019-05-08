///                MentOS, The Mentoring Operating system project
/// @file bitset.h
/// @brief Bitset data structure.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"
#include "stdbool.h"

/// @brief Bitset structure.
typedef struct {
	/// The internal data.
	unsigned char *data;
	/// The size of the bitset.
	size_t size;
} bitset_t;

// TODO: doxygen comment.
/// @brief
void bitset_init(bitset_t *set, size_t size);

// TODO: doxygen comment.
/// @brief
void bitset_free(bitset_t *set);

// TODO: doxygen comment.
/// @brief
void bitset_set(bitset_t *set, size_t bit);

// TODO: doxygen comment.
/// @brief
void bitset_clear(bitset_t *set, size_t bit);

// TODO: doxygen comment.
/// @brief
bool_t bitset_test(bitset_t *set, size_t bit);

// TODO: doxygen comment.
/// @brief
signed long bitset_find_first_unset_bit(bitset_t *set);
