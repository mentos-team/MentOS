/// @file compiler.h
/// @brief Definition of memory barriers.
/// @details 
/// Generally speaking, memory barriers prevent the compiler from merging or
/// refetching reads or writes. Ensuring that the compiler does not fold, spindle,
/// or otherwise mutilate accesses that either do not require ordering or that 
/// interact with an explicit memory barrier or atomic instruction that provides 
/// the required ordering.
///
/// Follows an extract from `LINUX KERNEL MEMORY BARRIERS` by: David Howells,
/// Paul E. McKenney, Will Deacon, Peter Zijlstra, available at:
///     https://www.kernel.org/doc/Documentation/memory-barriers.txt
///
/// Consider the following abstract model of the system:
/// @code
///             :                :
/// +-------+   :   +--------+   :   +-------+
/// | CPU 1 |<----->| Memory |<----->| CPU 2 |
/// +-------+   :   +--------+   :   +-------+
///     ^       :       ^        :       ^
///     |       :       |        :       |
///     |       :       v        :       |
///     |       :   +--------+   :       |
///     +---------->| Device |<----------+
///             :   +--------+   :
///             :                :
/// @endcode
/// Each CPU executes a program that generates memory access operations. In the
/// abstract CPU, memory operation ordering is very relaxed, and a CPU may actually
/// perform the memory operations in any order it likes, provided program causality
/// appears to be maintained. Similarly, the compiler may also arrange the
/// instructions it emits in any order it likes, provided it doesn't affect the
/// apparent operation of the program.
///
/// So in the above diagram, the effects of the memory operations performed by a
/// CPU are perceived by the rest of the system as the operations cross the
/// interface between the CPU and rest of the system (the dotted lines).
///
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Assign the value to the given variable.
#define WRITE_ONCE(var, val) (*((__volatile__ __typeof__(val) *)(&(var))) = (val))

/// @brief Read the value from the given variable.
#define READ_ONCE(var) (*((__volatile__ __typeof__(var) *)(&(var))))
