/// @file mman.c
/// @brief Functions for managing mappings in virtual address space.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/mman.h"
#include "errno.h"
#include "unistd.h"
#include "system/syscall_types.h"

#if 0

// _syscall6(void *, mmap, void *, addr, size_t, length, int, prot, int, flags, int, fd, off_t, offset)
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    long __res;
    __inline_syscall_6(__res, mmap, addr, length, prot, flags, fd, offset);
    __syscall_return(void *, __res);
}

// _syscall2(int, munmap, void *, addr, size_t, length)
int munmap(void *addr, size_t length)
{
    long __res;
    __inline_syscall_2(__res, munmap, addr, length);
    __syscall_return(int, __res);
}

#endif
