/// @file stack_helper.h
/// @brief Inline functions for safe stack manipulation with proper sequencing.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/// @brief Push a 32-bit value onto the stack, decrementing the stack pointer.
/// @param sp Pointer to 32-bit stack pointer (will be decremented).
/// @param value The 32-bit value to push.
static inline void stack_push_u32(uint32_t *sp, uint32_t value)
{
    *sp -= sizeof(uint32_t);
    __asm__ __volatile__("" ::: "memory");
    *(volatile uint32_t *)(*sp) = value;
    __asm__ __volatile__("" ::: "memory");
}

/// @brief Push a signed 32-bit value onto the stack, decrementing the stack pointer.
/// @param sp Pointer to 32-bit stack pointer (will be decremented).
/// @param value The signed 32-bit value to push.
static inline void stack_push_s32(uint32_t *sp, int32_t value)
{
    *sp -= sizeof(int32_t);
    __asm__ __volatile__("" ::: "memory");
    *(volatile int32_t *)(*sp) = value;
    __asm__ __volatile__("" ::: "memory");
}

/// @brief Push a pointer value onto the stack, decrementing the stack pointer.
/// @param sp Pointer to 32-bit stack pointer (will be decremented).
/// @param ptr The pointer value to push.
static inline void stack_push_ptr(uint32_t *sp, const void *ptr)
{
    *sp -= sizeof(uint32_t);
    __asm__ __volatile__("" ::: "memory");
    *(volatile uint32_t *)(*sp) = (uint32_t)ptr;
    __asm__ __volatile__("" ::: "memory");
}

/// @brief Push a single byte onto the stack, decrementing the stack pointer.
/// @param sp Pointer to 32-bit stack pointer (will be decremented).
/// @param byte The byte value to push.
static inline void stack_push_u8(uint32_t *sp, uint8_t byte)
{
    *sp -= sizeof(uint8_t);
    __asm__ __volatile__("" ::: "memory");
    *(volatile uint8_t *)(*sp) = byte;
    __asm__ __volatile__("" ::: "memory");
}

/// @brief Pop a 32-bit value from the stack, incrementing the stack pointer.
/// @param sp Pointer to 32-bit stack pointer (will be incremented).
/// @return The 32-bit value popped from the stack.
static inline uint32_t stack_pop_u32(uint32_t *sp)
{
    uint32_t value = *(volatile uint32_t *)(*sp);
    __asm__ __volatile__("" ::: "memory");
    *sp += sizeof(uint32_t);
    return value;
}

/// @brief Pop a signed 32-bit value from the stack, incrementing the stack pointer.
/// @param sp Pointer to 32-bit stack pointer (will be incremented).
/// @return The signed 32-bit value popped from the stack.
static inline int32_t stack_pop_s32(uint32_t *sp)
{
    int32_t value = *(volatile int32_t *)(*sp);
    __asm__ __volatile__("" ::: "memory");
    *sp += sizeof(int32_t);
    return value;
}

/// @brief Pop a pointer value from the stack, incrementing the stack pointer.
/// @param sp Pointer to 32-bit stack pointer (will be incremented).
/// @return The pointer value popped from the stack.
static inline void *stack_pop_ptr(uint32_t *sp)
{
    void *value = (void *)*(volatile uint32_t *)(*sp);
    __asm__ __volatile__("" ::: "memory");
    *sp += sizeof(uint32_t);
    return value;
}

/// @brief Push arbitrary data onto the stack, decrementing the stack pointer.
/// @param sp Pointer to 32-bit stack pointer (will be decremented by size).
/// @param data Pointer to data to push.
/// @param size Number of bytes to push.
static inline void stack_push_data(uint32_t *sp, const void *data, size_t size)
{
    *sp -= size;
    __asm__ __volatile__("" ::: "memory");
    memcpy((void *)*sp, data, size);
    __asm__ __volatile__("" ::: "memory");
}

/// @brief Pop arbitrary data from the stack, incrementing the stack pointer.
/// @param sp Pointer to 32-bit stack pointer (will be incremented by size).
/// @param data Pointer to buffer where popped data will be stored.
/// @param size Number of bytes to pop.
static inline void stack_pop_data(uint32_t *sp, void *data, size_t size)
{
    memcpy(data, (void *)*sp, size);
    __asm__ __volatile__("" ::: "memory");
    *sp += size;
}
