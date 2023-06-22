/// @file stdlib.h
/// @brief Useful generic functions and macros.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#ifndef __KERNEL__

#include "stddef.h"

#define EXIT_SUCCESS 0 ///< Successful execution of a program.
#define EXIT_FAILURE 1 ///< Unsuccessful execution of a program.

/// @brief Returns the number of usable bytes in the block pointed to by ptr.
/// @param ptr The pointer for which we want to retrieve the usable size.
/// @return The number of usable bytes in the block of allocated memory
///         pointed to by ptr. If ptr is not a valid pointer, 0 is returned.
size_t malloc_usable_size(void *ptr);

/// @brief Provides dynamically allocated memory.
/// @param size The amount of memory to allocate.
/// @return A pointer to the allocated memory.
void *malloc(unsigned int size);

/// @brief Allocates a block of memory for an array of num elements.
/// @param num  The number of elements.
/// @param size The size of an element.
/// @return A pointer to the allocated memory.
void *calloc(size_t num, size_t size);

/// @brief Reallocates the given area of memory.
/// @param ptr  The pointer to the memory to reallocate.
/// @param size The new size for the memory.
/// @return A pointer to the new portion of memory.
/// @details
/// It must be previously allocated by malloc(), calloc() or realloc() and
/// not yet freed with a call to free or realloc. Otherwise, the results
/// are undefined.
void *realloc(void *ptr, size_t size);

/// @brief Frees dynamically allocated memory.
/// @param ptr The pointer to the allocated memory.
void free(void *ptr);

/// @brief Cause an abnormal program termination with core-dump.
void abort();

/// @brief Tries to adds the variable to the environment.
/// @param name      Name of the variable.
/// @param value     Value of the variable.
/// @param overwrite Override existing variable value or not.
/// @return Zero on success, or -1 on error with errno indicating the cause.
int setenv(const char *name, const char *value, int overwrite);

/// @brief Tries to remove the variable from the environment.
/// @param name      Name of the variable.
/// @return Zero on success, or -1 on error with errno indicating the cause.
int unsetenv(const char *name);

/// @brief Returns the value of the given variable.
/// @param name Name of the variable.
/// @return A pointer to the value, or NULL if there is no match.
char *getenv(const char *name);

#endif

/// The maximum value returned by the rand function.
#define RAND_MAX ((1U << 31U) - 1U)

/// @brief Allows to set the seed of the random value generator.
/// @param x The new seed.
void srand(unsigned x);

/// @brief Generates a random unsigned integer between 0 and RAND_MAX.
/// @return the random value.
unsigned rand();

/// @brief Generates a random floating point number between 0 and 1.
/// @return the random value.
float randf();

/// @brief Generates a random integer between lb and ub.
/// @param lb the lower-bound value.
/// @param ub the upper-bound value.
/// @return the random value.
int randint(int lb, int ub);

/// @brief Generates a random unsigned between lb and ub.
/// @param lb the lower-bound value.
/// @param ub the upper-bound value.
/// @return the random value.
unsigned randuint(unsigned lb, unsigned ub);

/// @brief Generates a random float between lb and ub.
/// @param lb the lower-bound value.
/// @param ub the upper-bound value.
/// @return the random value.
float randfloat(float lb, float ub);
