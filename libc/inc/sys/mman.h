/// @file mman.h
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

/// @brief creates a new mapping in the virtual address space of the calling process.
/// @param addr the starting address for the new mapping.
/// @param length specifies the length of the mapping (which must be greater than 0).
/// @param prot describes the desired memory protection of the mapping (and must not conflict with the open mode of the file).
/// @param flags determines whether updates to the mapping are visible to other processes mapping the same region.
/// @param fd in case of file mapping, the file descriptor to use.
/// @param offset offset in the file, which must be a multiple of the page size PAGE_SIZE.
/// @return returns a pointer to the mapped area, -1 and errno is set.
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

/// @brief deletes the mappings for the specified address range.
/// @param addr the starting address.
/// @param length the length of the mapped area.
/// @return 0 on success, -1 on falure and errno is set.
int munmap(void *addr, size_t length);

#else

/// @brief creates a new mapping in the virtual address space of the calling process.
/// @param addr the starting address for the new mapping.
/// @param length specifies the length of the mapping (which must be greater than 0).
/// @param prot describes the desired memory protection of the mapping (and must not conflict with the open mode of the file).
/// @param flags determines whether updates to the mapping are visible to other processes mapping the same region.
/// @param fd in case of file mapping, the file descriptor to use.
/// @param offset offset in the file, which must be a multiple of the page size PAGE_SIZE.
/// @return returns a pointer to the mapped area, -1 and errno is set.
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

/// @brief deletes the mappings for the specified address range.
/// @param addr the starting address.
/// @param length the length of the mapped area.
/// @return 0 on success, -1 on falure and errno is set.
int sys_munmap(void *addr, size_t length);

#endif
