///                MentOS, The Mentoring Operating system project
/// @file port_io.c
/// @brief Byte I/O on ports prototypes.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "port_io.h"

inline uint8_t inportb(uint16_t port)
{
    unsigned char data = 0;
    __asm__ __volatile__("inb %%dx, %%al" : "=a" (data) : "d" (port));

    return data;
}

inline uint16_t inports(uint16_t port)
{
    uint16_t rv;
    __asm__ __volatile__("inw %1, %0" : "=a" (rv) : "dN" (port));

    return rv;
}

void inportsm(uint16_t port, uint8_t * data, unsigned long size)
{
    __asm__ __volatile__("rep insw" : "+D" (data), "+c" (size) : "d" (port) : "memory");
}

inline uint32_t inportl(uint16_t port)
{
    uint32_t rv;
    __asm__ __volatile__("inl %%dx, %%eax" : "=a" (rv) : "dN" (port));

    return rv;
}

inline void outportb(uint16_t port, uint8_t data)
{
    __asm__ __volatile__("outb %%al, %%dx"::"a" (data), "d" (port));
}

inline void outports(uint16_t port, uint16_t data)
{
    __asm__ __volatile__("outw %1, %0" : : "dN" (port), "a" (data));
}

void outportsm(uint16_t port, uint8_t *data, uint16_t size)
{
    asm volatile ("rep outsw" : "+S" (data), "+c" (size) : "d" (port));
}

inline void outportl(uint16_t port, uint32_t data)
{
    __asm__ __volatile__("outl %%eax, %%dx" : : "dN" (port), "a" (data));
}
