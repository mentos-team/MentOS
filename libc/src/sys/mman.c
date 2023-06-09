/// @file mman.c
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief Functions for managing mappings in virtual address space.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/mman.h"
#include "system/syscall_types.h"
#include "sys/unistd.h"
#include "sys/errno.h"

_syscall6(void *, mmap, void *, addr, size_t, length, int, prot, int, flags, int, fd, off_t, offset)

_syscall2(int, munmap, void *, addr, size_t, length)
