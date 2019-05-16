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

/// The maximum value returned by the rand function.
#define RAND_MAX ((1U << 31U) - 1U)

/// @brief Allows to set the seed of the random value generator.
void srand(int x);

/// @brief Generates a random value.
int rand();
