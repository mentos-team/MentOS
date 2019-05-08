///                MentOS, The Mentoring Operating system project
/// @file stdlib.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// @brief Malloc based on the number of elements.
void *calloc(size_t element_number, size_t element_size);

/// @brief Allows to set the seed of the random value generator.
void srand(int x);

void *malloc(unsigned int size);

void free(void * p);


#ifndef MS_RAND

/// @brief The maximum value of the random.
#define RAND_MAX ((1U << 31) - 1)

/// @brief Generates a random value.
int rand();

// MS rand
#else

#define RAND_MAX_32 ((1U << 31) - 1)

#define RAND_MAX ((1U << 15) - 1)

int rand();

#endif
