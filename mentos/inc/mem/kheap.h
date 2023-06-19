/// @file kheap.h
/// @brief  Interface for kernel heap functions, also provides a placement
///         malloc() for use before the heap is initialised.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "kernel.h"
#include "process/scheduler.h"

/// @brief User malloc.
/// @param addr This argument is treated as an address of a dynamically
///             allocated memory if falls inside the process heap area.
///             Otherwise, it is treated as an amount of memory that
///             should be allocated.
/// @return NULL if there is no more memory available or we were freeing
///         a previously allocated memory area, the address of the
///         allocated space otherwise.
void *sys_brk(void *addr);
