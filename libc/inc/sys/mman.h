/// @file mman.h
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief Functions for managing mappings in virtual address space.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

#define PROT_READ  0x1 ///< Page can be read.
#define PROT_WRITE 0x2 ///< Page can be written.
#define PROT_EXEC  0x4 ///< Page can be executed.

#define MAP_SHARED  0x01 ///< The memory is shared.
#define MAP_PRIVATE 0x02 ///< The memory is private.

#ifndef __KERNEL__

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

int munmap(void *addr, size_t length);

#else

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

int sys_munmap(void *addr, size_t length);

#endif
