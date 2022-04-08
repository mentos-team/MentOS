/// @file array.h
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#ifdef __KERNEL__
/// Function for allocating memory for the array.
#define ARRAY_ALLOC kmalloc
/// Function for freeing the memory for the array.
#define ARRAY_FREE  kfree
#else
/// Function for allocating memory for the array.
#define ARRAY_ALLOC malloc
/// Function for freeing the memory for the array.
#define ARRAY_FREE  free
#endif

/// @brief Declares a new dynamic-size array structure.
#define DECLARE_ARRAY(type, name)                                                     \
    typedef struct arr_##name##_t {                                                   \
        const unsigned size;                                                          \
        type *buffer;                                                                 \
    } arr_##name##_t;                                                                 \
    arr_##name##_t alloc_arr_##name(unsigned len)                                     \
    {                                                                                 \
        arr_##name##_t a = { len, len > 0 ? ARRAY_ALLOC(sizeof(type) * len) : NULL }; \
        memset(a.buffer, 0, sizeof(type) * len);                                      \
        return a;                                                                     \
    }                                                                                 \
    static inline void free_arr_##name(arr_##name##_t *arr)                           \
    {                                                                                 \
        ARRAY_FREE(arr->buffer);                                                      \
    }

#undef ARRAY_ALLOC malloc
#undef ARRAY_FREE free