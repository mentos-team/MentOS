/// @file stdlib.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stdlib.h"

/// Seed used to generate random numbers.
static int rseed = 0;

void srand(int x)
{
    rseed = x;
}

int rand()
{
    return rseed = (rseed * 1103515245U + 12345U) & RAND_MAX;
}
