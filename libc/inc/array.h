/// @file array.h
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Declares a new dynamic-size array structure with external allocation.
///
/// @details This macro defines a structure for a dynamic array and functions to
/// allocate and free it, where the allocation and deallocation functions are
/// provided as arguments.
///
/// @param type The data type of the array elements.
/// @param name The base name for the generated structure and functions.
#define DECLARE_ARRAY(type, name)                                                                                      \
    typedef struct arr_##name##{                                                                                       \
        const unsigned size;                                                                                           \
        type *buffer;                                                                                                  \
    } arr_##name##_t;                                                                                                  \
                                                                                                                       \
    static inline arr_##name##_t alloc_arr_##name(unsigned len, void *(*alloc_func)(size_t))                           \
    {                                                                                                                  \
        arr_##name##_t a = {len, len > 0 ? alloc_func(sizeof(type) * len) : NULL};                                     \
        if (a.buffer)                                                                                                  \
            memset(a.buffer, 0, sizeof(type) * len);                                                                   \
        return a;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static inline void free_arr_##name(arr_##name##_t *arr, void (*free_func)(void *)) { free_func(arr->buffer); }
