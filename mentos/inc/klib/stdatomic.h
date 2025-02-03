/// @file stdatomic.h
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "klib/compiler.h"

/// @brief Standard structure for atomic operations (see below for volatile
/// explanation).
typedef volatile unsigned atomic_t;

/// @brief The prefix used to lock.
#define LOCK_PREFIX "\n\tlock; "

/// @brief Compile read-write barrier.
#define barrier() __asm__ __volatile__("" : : : "memory")

/// @brief Pause instruction to prevent excess processor bus usage.
#define cpu_relax() __asm__ __volatile__("pause\n" : : : "memory")

/// @brief Atomically compares and exchanges a value.
/// @details If *ptr equals old_val, this function sets *ptr to new_val. Used
/// for conditional updates in lock-free data structures.
/// @param[in,out] ptr Pointer to the atomic variable.
/// @param[in] old_val Expected current value of *ptr.
/// @param[in] new_val Value to set if *ptr equals old_val.
/// @return The original value of *ptr. If this equals old_val, the exchange was
/// successful.
static inline atomic_t atomic_cmpxchg_and_test(volatile atomic_t *ptr, atomic_t old_val, atomic_t new_val)
{
    atomic_t prev;
    __asm__ __volatile__(LOCK_PREFIX "cmpxchgl %2, %1" // Compare *ptr with %eax; if equal, set to new_val
                         : "=a"(prev),
                           "+m"(*ptr)                 // Output: prev holds *ptr, *ptr modified if equal
                         : "r"(new_val), "0"(old_val) // Inputs: new_val, old_val in %eax
                         : "memory");                 // Clobbered: memory to prevent reordering
    return prev;                                      // Return the original value of *ptr
}

/// @brief Atomically sets *ptr to a given value.
/// @details This function sets *ptr to value unconditionally and returns the
/// original value. Useful for atomic resets and state changes.
/// @param ptr Pointer to the atomic variable.
/// @param value New value to set at *ptr.
/// @return The previous value of *ptr.
static inline int atomic_set_and_test(atomic_t *ptr, int value)
{
    __asm__ __volatile__(LOCK_PREFIX               // Lock prefix for atomicity
                         "xchgl %0, %1"            // Exchange value with *ptr
                         : "+r"(value), "+m"(*ptr) // Input + Output
                         :                         // No additional input-only constraints
                         : "memory");              // Clobbers memory to prevent reordering
    return value;                                  // Return the original value of *ptr
}

/// @brief Atomically set the value pointed by `ptr` to `value`.
/// @param ptr the pointer we are working with.
/// @param value the value we need to set.
static inline void atomic_set(atomic_t *ptr, int value) { atomic_set_and_test(ptr, value); }

/// @brief Atomically read the value pointed by `ptr`.
/// @param ptr the pointer we are working with.
/// @return the value we read.
static inline int atomic_read(const atomic_t *ptr) { return READ_ONCE(*ptr); }

/// @brief Atomically add `value` to the value pointed by `ptr`.
/// @param ptr the pointer we are working with.
/// @param value the value we need to add.
/// @return the result of the operation.
static inline int atomic_add(atomic_t *ptr, int value)
{
    // The + in "+r" and "+m" denotes a read-modify-write operand.
    __asm__ __volatile__(LOCK_PREFIX               // Lock
                         "xaddl %0, %1"            // Instruction
                         : "+r"(value), "+m"(*ptr) // Input + Output
                         :                         // No input-only
                         : "memory");              // Side effects
    return value;
}

/// @brief Atomically subtract `value` from the value pointed by `ptr`.
/// @param ptr the pointer we are working with.
/// @param value the value we need to subtract.
/// @return the result of the operation.
static inline int atomic_sub(atomic_t *ptr, int value) { return atomic_add(ptr, -value); }

/// @brief Atomically increment the value at `ptr`.
/// @param ptr the pointer we are working with.
/// @return the result of the operation.
static inline int atomic_inc(atomic_t *ptr) { return atomic_add(ptr, 1); }

/// @brief Atomically decrement the value at `ptr`.
/// @param ptr the pointer we are working with.
/// @return the result of the operation.
static inline int atomic_dec(atomic_t *ptr) { return atomic_add(ptr, -1); }

/// @brief Atomically add `value` to `ptr` and checks if the result is negative.
/// @param ptr the pointer we are working with.
/// @param value the value we need to add.
/// @return true if the result is negative, false otherwise.
static inline int atomic_add_negative(atomic_t *ptr, int value) { return atomic_add(ptr, value) < 0; }

/// @brief Atomically subtract `value` from `ptr` and checks if the result is zero.
/// @param ptr the pointer we are working with.
/// @param value the value we need to subtract.
/// @return true if the result is zero, false otherwise.
static inline int atomic_sub_and_test(atomic_t *ptr, int value) { return atomic_sub(ptr, value) == 0; }

/// @brief Atomically increment `ptr` and checks if the result is zero.
/// @param ptr the pointer we are working with.
/// @return true if the result is zero, false otherwise.
static inline int atomic_inc_and_test(atomic_t *ptr) { return atomic_inc(ptr) == 0; }

/// @brief Atomically decrement `ptr` and checks if the result is zero.
/// @param ptr the pointer we are working with.
/// @return true if the result is zero, false otherwise.
static inline int atomic_dec_and_test(atomic_t *ptr) { return atomic_dec(ptr) == 0; }

/// @brief Atomically sets a bit in memory, using Bit Test And Set (bts).
/// @param offset The offset to the bit.
/// @param base   The base address.
static inline void set_bit(int offset, volatile unsigned long *base)
{
    __asm__ __volatile__("btsl %[offset], %[base]" : [base] "=m"(*(volatile long *)base) : [offset] "Ir"(offset));
}

/// @brief Atomically clears a bit in memory.
/// @param offset The offset to the bit.
/// @param base   The base address.
static inline void clear_bit(int offset, volatile unsigned long *base)
{
    __asm__ __volatile__("btrl %[offset],%[base]" : [base] "=m"(*(volatile long *)base) : [offset] "Ir"(offset));
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
                         : [base] "m"(*(volatile long *)base), [offset] "Ir"(offset));
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
