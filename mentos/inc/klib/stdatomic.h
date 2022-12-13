/// @file stdatomic.h
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"
#include "stdbool.h"
#include "klib/compiler.h"

/// @brief Standard structure for atomic operations (see below
///        for volatile explanation).
typedef volatile unsigned atomic_t;

/// @brief The prefix used to lock.
#define LOCK_PREFIX "\n\tlock; "

/// @brief Compile read-write barrier.
#define barrier() __asm__ __volatile__("" \
                                       :  \
                                       :  \
                                       : "memory")

/// @brief Pause instruction to prevent excess processor bus usage.
#define cpu_relax() __asm__ __volatile__("pause\n" \
                                         :         \
                                         :         \
                                         : "memory")

/// @brief Atomically sets the value of ptr with the value of x.
///
/// @param ptr Pointer to the atomic variable to set.
/// @param x   The value to set.
/// @return The final value of the atomic variable.
inline static int32_t atomic_set_and_test(atomic_t *ptr, int32_t x)
{
    __asm__ __volatile__("xchgl %0,%1"       // instruction
                         : "=r"(x)           // outputs
                         : "m"(*ptr), "0"(x) // inputs
                         : "memory");        // side effects
    return x;
}

/// @brief Atomically set ptr equal to i.
inline static void atomic_set(atomic_t *ptr, int32_t i)
{
    atomic_set_and_test(ptr, i);
}

/// @brief Atomically read the integer value of ptr.
inline static int32_t atomic_read(const atomic_t *ptr)
{
    return READ_ONCE(*ptr);
}

/// @brief Atomically add i to ptr.
inline static int32_t atomic_add(atomic_t const *ptr, int32_t i)
{
    int result = i;
    __asm__ __volatile__(LOCK_PREFIX "xaddl %0, %1"
                         : "=r"(i)
                         : "m"(ptr), "0"(i)
                         : "memory", "cc");
    return result + i;
}

/// @brief Atomically subtract i from ptr.
inline static int32_t atomic_sub(atomic_t *ptr, int32_t i)
{
    return atomic_add(ptr, -i);
}

/// @brief Atomically add one to ptr.
inline static int32_t atomic_inc(atomic_t *ptr)
{
    return atomic_add(ptr, 1);
}

/// @brief Atomically subtract one from ptr.
inline static int32_t atomic_dec(atomic_t *ptr)
{
    return atomic_add(ptr, -1);
}

/// @brief Atomically add i to ptr and return true if the result is negative;
///        otherwise false.
inline static bool_t atomic_add_negative(atomic_t *ptr, int32_t i)
{
    return (bool_t)(atomic_add(ptr, i) < 0);
}

/// @brief Atomically subtract i from ptr and return true if the result is
///        zero; otherwise false.
inline static bool_t atomic_sub_and_test(atomic_t *ptr, int32_t i)
{
    return (bool_t)(atomic_sub(ptr, i) == 0);
}

/// @brief Atomically increment ptr by one and return true if the result is
///        zero; false otherwise.
inline static int32_t atomic_inc_and_test(atomic_t *ptr)
{
    return (bool_t)(atomic_inc(ptr) == 0);
}

/// @brief Atomically decrement ptr by one and return true if zero; false
///        otherwise.
inline static int32_t atomic_dec_and_test(atomic_t *ptr)
{
    return (bool_t)(atomic_dec(ptr) == 0);
}

/// @brief Atomically sets a bit in memory, using Bit Test And Set (bts).
/// @param offset The offset to the bit.
/// @param base   The base address.
static inline void set_bit(int offset, volatile unsigned long *base)
{
    __asm__ __volatile__("btsl %[offset],%[base]"
                         : [base] "=m"(*(volatile long *)base)
                         : [offset] "Ir"(offset));
}

/// @brief Atomically clears a bit in memory.
/// @param offset The offset to the bit.
/// @param base   The base address.
static inline void clear_bit(int offset, volatile unsigned long *base)
{
    __asm__ __volatile__("btrl %[offset],%[base]"
                         : [base] "=m"(*(volatile long *)base)
                         : [offset] "Ir"(offset));
}

/// @brief Atomically tests a bit in memory.
/// @param offset The offset to the bit.
/// @param base   The base address.
/// @return 1 if the bit is set, 0 otherwise.
static inline int test_bit(int offset, volatile unsigned long *base)
{
    int old = 0;
    __asm__ __volatile__("btl %[offset],%[base]\n" // Bit Test
                         "sbbl %[old],%[old]\n"    // Return the previous value.
                         : [old] "=r"(old)
                         : [base] "m"(*(volatile long *)base),
                           [offset] "Ir"(offset));
    return old;
}

// == Volatile Variable =======================================================
// In C, and consequently C++, the volatile keyword was intended to:
//     - allow access to memory-mapped I/O devices
//     - allow uses of variables between setjmp and longjmp
//     - allow uses of sig_atomic_t variables in signal handlers.
//
// Operations on volatile variables are not atomic, nor do they establish
// a proper happens-before relationship for threading like with the
// `__asm__` inline blocks.
// This is specified in the relevant standards (C, C++, POSIX, WIN32), and
// volatile variables are not thread-safe in the vast majority of current
// implementations.
// Thus, the usage of volatile keyword as a portable synchronization mechanism
// is discouraged by many C/C++ groups.

// == xchg/xchgl ==============================================================
