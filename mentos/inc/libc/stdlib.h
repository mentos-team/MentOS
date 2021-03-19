///                MentOS, The Mentoring Operating system project
/// @file stdlib.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// @brief Malloc based on the number of elements.
void *calloc(size_t element_number, size_t element_size);

void *malloc(unsigned int size);

void free(void * p);

/// @brief Allocate a matrix with n rows of a specific size.
/// @param n     Number of matrix rows.
/// @param size  Size of each matrix row.
/// @return Matrix pointer.
void **mmalloc(size_t n, size_t size);

/// @brief Free a matrix of n rows.
/// @param src Matrix to free.
/// @param n   Number of matrix rows.
void mfree(void **src, size_t n);

/// The maximum value returned by the rand function.
#define RAND_MAX ((1U << 31U) - 1U)

/// @brief Allows to set the seed of the random value generator.
void srand(int x);

/// @brief Generates a random value.
int rand();
