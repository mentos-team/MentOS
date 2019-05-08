///                MentOS, The Mentoring Operating system project
/// @file ordered_array.c
/// @brief Interface for creating, inserting and deleting from ordered arrays.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "ordered_array.h"
#include "string.h"
#include "assert.h"
#include "stdlib.h"

int8_t standard_lessthan_predicate(array_type_t a, array_type_t b)
{
    return (a < b);
}

ordered_array_t create_ordered_array(uint32_t max_size,
                                     lessthan_predicate_t less_than)
{
    ordered_array_t to_ret;
    to_ret.array = malloc(max_size * sizeof(array_type_t));
    memset(to_ret.array, 0, max_size * sizeof(array_type_t));
    to_ret.size = 0;
    to_ret.max_size = max_size;
    to_ret.less_than = less_than;

    return to_ret;
}

ordered_array_t place_ordered_array(void * addr,
                                    uint32_t max_size,
                                    lessthan_predicate_t less_than)
{
    ordered_array_t to_ret;
    to_ret.array = (array_type_t *) addr;
    memset(to_ret.array, 0, max_size * sizeof(array_type_t));
    to_ret.size = 0;
    to_ret.max_size = max_size;
    to_ret.less_than = less_than;

    return to_ret;
}

void destroy_ordered_array(ordered_array_t * array)
{
    free(array->array);
}

void insert_ordered_array(array_type_t item, ordered_array_t * array)
{
    assert(array->less_than);

    uint32_t iterator = 0;
    while (iterator < array->size && array->less_than(array->array[iterator],
                                                      item))
    {
        iterator++;
    }

    // Just add at the end of the array.
    if (iterator == array->size)
    {
        array->array[array->size++] = item;
    }
    else
    {
        array_type_t tmp = array->array[iterator];
        array->array[iterator] = item;

        while (iterator < array->size)
        {
            iterator++;
            array_type_t tmp2 = array->array[iterator];
            array->array[iterator] = tmp;
            tmp = tmp2;
        }
        array->size++;
    }
}

array_type_t lookup_ordered_array(uint32_t i, ordered_array_t * array)
{
    assert(i < array->size);

    return array->array[i];
}

void remove_ordered_array(uint32_t i, ordered_array_t * array)
{
    while (i < array->size)
    {
        array->array[i] = array->array[i + 1];
        i++;
    }

    array->size--;
}
