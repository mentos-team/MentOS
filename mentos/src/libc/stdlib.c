///                MentOS, The Mentoring Operating system project
/// @file stdlib.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "syscall.h"
#include "stdlib.h"
#include "string.h"

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
	if (ptr) {
		memset(ptr, 0, element_number * element_size);
	}

	return ptr;
}

void **mmalloc(size_t n, size_t size)
{
    void **ret = (void **) malloc(n * sizeof(void *));
    for (size_t i = 0; i < n; i++)
    {
        *(ret + i) = malloc(size);
    }
    return ret;
}

void mfree(void **src, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        free(*(src + i));
    }
    free(src);
}

/// Seed used to generate random numbers.
static int rseed = 0;

inline void srand(int x)
{
	rseed = x;
}

/// @brief Returns a pseudo-random integral number in the range
///        between 0 and RAND_MAX.
inline int rand()
{
	return rseed = (rseed * 1103515245U + 12345U) & RAND_MAX;
}
