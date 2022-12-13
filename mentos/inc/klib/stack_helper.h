/// @file stack_helper.h
/// @brief Couple of macros that help accessing the stack.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Access the value of the pointer.
#define __ACCESS_PTR(type, ptr) (*(type *)(ptr))
/// @brief Moves the pointer down.
#define __MOVE_PTR_DOWN(type, ptr) ((ptr) -= sizeof(type))
/// @brief Moves the pointer up.
#define __MOVE_PTR_UP(type, ptr) ((ptr) += sizeof(type))
/// @brief First, it moves the pointer down, and then it pushes the value at that memory location.
#define PUSH_VALUE_ON_STACK(ptr, value) (__ACCESS_PTR(__typeof__(value), __MOVE_PTR_DOWN(__typeof__(value), ptr)) = (value))
/// @brief First, it access the value at the given memory location, and then it moves the pointer up.
#define POP_VALUE_FROM_STACK(value, ptr) ({value = __ACCESS_PTR(__typeof__(value), ptr); __MOVE_PTR_UP(__typeof__(value), ptr); })
