/// @file mman.c
/// @brief Functions for managing mappings in virtual address space.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/mman.h"
#include "errno.h"
#include "unistd.h"
#include "system/syscall_types.h"

_syscall6(void *, mmap, void *, addr, size_t, length, int, prot, int, flags, int, fd, off_t, offset)

_syscall2(int, munmap, void *, addr, size_t, length)
