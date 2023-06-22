/// @file stdlib.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stdlib.h"

/// Seed used to generate random numbers.
static unsigned rseed = 0;

void srand(unsigned x)
{
    rseed = x;
}

unsigned rand()
{
    return rseed = (rseed * 1103515245U + 12345U) & RAND_MAX;
}

float randf()
{
    return ((float)rand() / (float)(RAND_MAX));
}

int randint(int lb, int ub)
{
    return lb + ((int)rand() % (ub - lb + 1));
}

unsigned randuint(unsigned lb, unsigned ub)
{
    return lb + (rand() % (ub - lb + 1));
}

float randfloat(float lb, float ub)
{
    return lb + (randf() * (ub - lb));
}
