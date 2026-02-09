///                MentOS, The Mentoring Operating system project
/// @file arr_math.h
/// @brief Array arithmetic operations source file-
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdio.h"
#include "stdlib.h"
#include "stdio.h"
#include "stdint.h"
#include "stdbool.h"

/// @brief Initialize all memory cells of destination array with a value.
/// @param dst Pointer of destination array.
/// @param value Value to use for array inizialization.
/// @param length Length of destination array. 
/// @return The pointer of the destination array.
uint32_t *all(uint32_t *dst, uint32_t value, size_t length);

/// @brief Element wise array subtraction.
/// @param left Pointer of left operator and destination array.
/// @param right Pointer of right operator array.
/// @param length Length of arrays. 
/// @return The pointer of the destination array.
/// E.g. [4, 5, 6] - [1, 2, 3] = [3, 3, 3]
uint32_t *arr_sub(uint32_t *left, const uint32_t *right, size_t length);

/// @brief Element wise array addition.
/// @param left Pointer of left operator and destination array.
/// @param right Pointer of right operator array.
/// @param length Length of arrays. 
/// @return The pointer of the destination array.
/// E.g. [4, 5, 6] + [1, 2, 3] = [5, 7, 9]
uint32_t *arr_add(uint32_t *left, const uint32_t *right, size_t length);

/// @brief Check if at least one element of an array is greater than another 
/// array in relation to the respective index.
/// @param left Pointer of left operator array.
/// @param right Pointer of right operator array.
/// @param length Length of arrays. 
/// @return True if there is an element of the left array greater than the 
/// respective right one, otherwise false.
/// E.g. [1, 1, 6] g_any [1, 2, 3] = true
///      [1, 1, 3] g_any [1, 2, 3] = false
bool_t arr_g_any(const uint32_t *left, const uint32_t *right, size_t length);

/// @brief Check if at least one element of an array is greater or equal than 
/// another array in relation to the respective index.
/// @param left Pointer of left operator array.
/// @param right Pointer of right operator array.
/// @param length Length of arrays. 
/// @return True if there is an element of the left array greater or equal than 
/// the respective right one, otherwise false.
/// E.g. [1, 1, 6] ge_any [1, 2, 3] = true
///      [0, 1, 3] ge_any [1, 2, 3] = true
///      [0, 1, 2] ge_any [1, 2, 3] = false
bool_t arr_ge_any(const uint32_t *left, const uint32_t *right, size_t length);

/// @brief Check if at least one element of an array is less than another 
/// array in relation to the respective index.
/// @param left Pointer of left operator array.
/// @param right Pointer of right operator array.
/// @param length Length of arrays.
/// @return True if there is an element of the left array less than the 
/// respective right one, otherwise false.
/// E.g. [1, 2, 3] l_any [1, 1, 6] = true
///      [1, 2, 3] l_any [0, 1, 3] = false
bool_t arr_l_any(const uint32_t *left, const uint32_t *right, size_t length);

/// @brief Check if at least one element of an array is less or equal than 
/// another array in relation to the respective index.
/// @param left Pointer of left operator array.
/// @param right Pointer of right operator array.
/// @param length Length of arrays. 
/// @return True if there is an element of the left array less or equal than 
/// the respective right one, otherwise false.
/// E.g. [1, 2, 3] le_any [1, 1, 6] = true
///      [1, 2, 3] le_any [0, 1, 3] = true
///      [1, 2, 3] le_any [0, 1, 2] = false
bool_t arr_le_any(const uint32_t *left, const uint32_t *right, size_t length);

/// @brief Check if each element of an array is greater than another array in 
/// relation to the respective index.
/// @param left Pointer of left operator array.
/// @param right Pointer of right operator array.
/// @param length Length of arrays.
/// @return True if all elements of the left array are greater than the 
/// respective right one, otherwise false.
/// E.g. [2, 3, 4] g_all [1, 2, 3] = true
///      [2, 3, 3] g_all [1, 2, 3] = false
bool_t arr_g(const uint32_t *left, const uint32_t *right, size_t length);

/// @brief Check if each element of an array is greater or equal than another 
/// array in relation to the respective index.
/// @param left Pointer of left operator array.
/// @param right Pointer of right operator array.
/// @param length Length of arrays.
/// @return True if all elements of the left array are greater or equal than the 
/// respective right one, otherwise false.
/// E.g. [2, 3, 4] ge_all [1, 2, 3] = true
///      [2, 3, 3] ge_all [1, 2, 3] = true
///      [2, 3, 3] ge_all [1, 2, 4] = false
bool_t arr_ge(const uint32_t *left, const uint32_t *right, size_t length);

/// @brief Check if each element of an array is less than another array in 
/// relation to the respective index.
/// @param left Pointer of left operator array.
/// @param right Pointer of right operator array.
/// @param length Length of arrays.
/// @return True if all elements of the left array are less than the 
/// respective right one, otherwise false.
/// E.g. [1, 2, 3] l_all [2, 3, 4] = true
///      [1, 2, 3] l_all [2, 3, 3] = false
bool_t arr_l(const uint32_t *left, const uint32_t *right, size_t length);

/// @brief Check if each element of an array is less or equal than another 
/// array in relation to the respective index.
/// @param left Pointer of left operator array.
/// @param right Pointer of right operator array.
/// @param length Length of arrays.
/// @return True if all elements of the left array are less or equal than the 
/// respective right one, otherwise false.
/// E.g. [1, 2, 3] le_all [2, 3, 4] = true
///      [1, 2, 3] le_all [2, 3, 3] = true
///      [1, 2, 4] le_all [2, 3, 3] = false
bool_t arr_le(const uint32_t *left, const uint32_t *right, size_t length);

/// @brief Check if each element of an array is equal than another array in 
/// relation to the respective index.
/// @param left Pointer of left operator array.
/// @param right Pointer of right operator array.
/// @param length Length of arrays.
/// @return True if all elements of the left array are equal than the respective 
/// right one, otherwise false.
/// E.g. [1, 2, 3] e_all [1, 2, 3] = true
///      [1, 2, 3] e_all [1, 2, 1] = false
bool_t arr_e(const uint32_t *left, const uint32_t *right, size_t length);

/// @brief Check if at least one element of an array is not equal than another 
/// array in relation to the respective index.
/// @param left Pointer of left operator array.
/// @param right Pointer of right operator array.
/// @param length Length of arrays.
/// @return True if there is an element of the left array not equal than the 
/// respective right one, otherwise false.
/// E.g. [1, 2, 3] e_all [1, 2, 1] = true
///      [1, 2, 3] e_all [1, 2, 3] = false
bool_t arr_ne(const uint32_t *left, const uint32_t *right, size_t length);
