///                MentOS, The Mentoring Operating system project
/// @file stdatomic.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"
#include "stdbool.h"
#include "compiler.h"

/// @brief Standard structure for atomic operations.
typedef volatile unsigned atomic_t;

/// @brief At declaration, initialize an atomic_t to i.
#define ATOMIC_INIT(i)                                                         \
	{                                                                          \
		(i)                                                                    \
	}

/// @brief The prefix used to lock.
#define LOCK_PREFIX "\n\tlock; "

/// @brief Compile read-write barrier.
#define barrier() asm volatile("" : : : "memory")

/// @brief Pause instruction to prevent excess processor bus usage.
#define cpu_relax() asm volatile("pause\n" : : : "memory")

// TODO: doxygen comment.
inline static int32_t atomic_set_and_test(atomic_t const *ptr, int32_t i)
{
	__asm__ __volatile__("xchgl %0,%1"
						 : "=r"(i)
						 : "m"(*(volatile unsigned *)ptr), "0"(i)
						 : "memory");
	// __asm__ __volatile__("xchgl %0, %1" : "=r"(i) : "m"(ptr), "0"(i) : "memory");
	return i;
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
	asm volatile(LOCK_PREFIX "xaddl %0, %1"
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
