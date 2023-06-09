/// @file mman.h
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief Functions for managing mappings in virtual address space.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

#ifndef __KERNEL__

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

int munmap(void *addr, size_t length);

#else

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

int sys_munmap(void *addr, size_t length);

#endif
