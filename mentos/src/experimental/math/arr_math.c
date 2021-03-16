/**
 * @author Mirco De Marchi
 * @date   2/02/2021
 * @brief  Source file for arithmetic operations between arrays.
 * @copyright (c) University of Verona
 */

#include "arr_math.h"
//------------------------------------------------------------------------------

uint32_t *all(uint32_t *dst, uint32_t value, size_t length)
{
    for (size_t i = 0; i < length; i++) 
    {
        *(dst + i) = value;
    }
    return dst;
}

uint32_t *arr_sub(uint32_t *left, uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) 
    {
        *(left + i) -= *(right + i);
    }
    return left;
}

uint32_t *arr_add(uint32_t *left, uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) 
    {
        *(left + i) += *(right + i);
    }
    return left;
}

bool_t arr_g_any(uint32_t *left, uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) 
    {
        if (*(left + i) > *(right + i))
        {
            return true;
        }
    }
    return false;
}

bool_t arr_ge_any(uint32_t *left, uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) 
    {
        if (*(left + i) >= *(right + i))
        {
            return true;
        }
    }
    return false;
}

bool_t arr_l_any(uint32_t *left, uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) 
    {
        if (*(left + i) < *(right + i))
        {
            return true;
        }
    }
    return false;
}

bool_t arr_le_any(uint32_t *left, uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) 
    {
        if (*(left + i) <= *(right + i))
        {
            return true;
        }
    }
    return false;
}

bool_t arr_g(uint32_t *left, uint32_t *right, size_t length)
{
    return !arr_le_any(left, right, length);
}

bool_t arr_ge(uint32_t *left, uint32_t *right, size_t length)
{
    return !arr_l_any(left, right, length);
}

bool_t arr_l(uint32_t *left, uint32_t *right, size_t length)
{
    return !arr_ge_any(left, right, length);
}

bool_t arr_le(uint32_t *left, uint32_t *right, size_t length)
{
    return !arr_g_any(left, right, length);
}

bool_t arr_e(uint32_t *left, uint32_t *right, size_t length)
{
    for (size_t i = 0; i < length; i++) 
    {
        if (*(left + i) != *(right + i))
        {
            return false;
        }
    }
    return true;
}

bool_t arr_ne(uint32_t *left, uint32_t *right, size_t length)
{
    return !arr_e(left, right, length);
}
