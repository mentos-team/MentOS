///                MentOS, The Mentoring Operating system project
/// @file stdlib.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "syscall.h"
#include "stdlib.h"
#include "string.h"

/// Used to align the memory.
#define ALIGN(x) \
    (((x) + (sizeof(size_t) - 1)) & ~(sizeof(size_t) - 1))


void *malloc(unsigned int size)
{
    void *_res;
    DEFN_SYSCALL1(_res, __NR_brk, size);

    return _res;
}

void free(void *p)
{
    int _res;
    DEFN_SYSCALL1(_res, __NR_free, p);
}


void *calloc(size_t element_number, size_t element_size)
{
    void *ptr = malloc(element_number * element_size);
    if (ptr)
    {
        memset(ptr, 0, element_number * element_size);
    }

    return ptr;
}


/// Seed used to generate random numbers.
int rseed = 0;

inline void srand(int x)
{
    rseed = x;
}

#ifndef MS_RAND

/// The maximum value returned by the rand function.
#define RAND_MAX ((1U << 31) - 1)

/// @brief Returns a pseudo-random integral number in the range
///        between 0 and RAND_MAX.
inline int rand()
{
    return rseed = (rseed * 1103515245 + 12345) & RAND_MAX;
}

// MS rand.
#else
/// The maximum 32bit value returned by the rand function.
#define RAND_MAX_32 ((1U << 31) - 1)

/// The maximum value returned by the rand function.
#define RAND_MAX ((1U << 15) - 1)

/// @brief Returns a pseudo-random integral number in the range
///        between 0 and RAND_MAX.
inline int rand()
{
    return (rseed = (rseed * 214013 + 2531011) & RAND_MAX_32) >> 16;
}
<<<<<<< HEAD
#endif
