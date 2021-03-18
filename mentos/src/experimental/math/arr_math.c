///                MentOS, The Mentoring Operating system project
/// @file arr_math.c
/// @brief Array arithmetic operations source file-
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "arr_math.h"

uint32_t *all(uint32_t *dst, uint32_t value, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        *(dst + i) = value;
    }
    return dst;
}

uint32_t *arr_sub(uint32_t *left, const uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        *(left + i) -= *(right + i);
    }
    return left;
}

uint32_t *arr_add(uint32_t *left, const uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        *(left + i) += *(right + i);
    }
    return left;
}

bool_t arr_g_any(const uint32_t *left, const uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        if (*(left + i) > *(right + i)) {
            return true;
        }
    }
    return false;
}

bool_t arr_ge_any(const uint32_t *left, const uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        if (*(left + i) >= *(right + i)) {
            return true;
        }
    }
    return false;
}

bool_t arr_l_any(const uint32_t *left, const uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        if (*(left + i) < *(right + i)) {
            return true;
        }
    }
    return false;
}

bool_t arr_le_any(const uint32_t *left, const uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        if (*(left + i) <= *(right + i)) {
            return true;
        }
    }
    return false;
}

bool_t arr_g(const uint32_t *left, const uint32_t *right, size_t length)
{
    return !arr_le_any(left, right, length);
}

bool_t arr_ge(const uint32_t *left, const uint32_t *right, size_t length)
{
    return !arr_l_any(left, right, length);
}

bool_t arr_l(const uint32_t *left, const uint32_t *right, size_t length)
{
    return !arr_ge_any(left, right, length);
}

bool_t arr_le(const uint32_t *left, const uint32_t *right, size_t length)
{
    return !arr_g_any(left, right, length);
}

bool_t arr_e(const uint32_t *left, const uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        if (*(left + i) != *(right + i)) {
            return false;
        }
    }
    return true;
}

bool_t arr_ne(const uint32_t *left, const uint32_t *right, size_t length)
{
    return !arr_e(left, right, length);
}
