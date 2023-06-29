/// @file port_io.c
/// @brief Byte I/O on ports prototypes.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "io/port_io.h"

inline uint8_t inportb(uint16_t port)
{
    uint8_t result;
    __asm__ __volatile__("inb %%dx, %%al"
                         : "=a"(result)
                         : "dN"(port)
                         : "memory");
    return result;
}

inline uint16_t inports(uint16_t port)
{
    uint16_t result;
    __asm__ __volatile__("inw %1, %0"
                         : "=a"(result)
                         : "dN"(port)
                         : "memory");
    return result;
}

inline uint32_t inportl(uint16_t port)
{
    uint32_t result;
    __asm__ __volatile__("inl %%dx, %%eax"
                         : "=a"(result)
                         : "dN"(port)
                         : "memory");
    return result;
}

inline void outportb(uint16_t port, uint8_t value)
{
    __asm__ __volatile__("outb %%al, %%dx"
                         :
                         : "a"(value), "dN"(port)
                         : "memory");
}

inline void outports(uint16_t port, uint16_t value)
{
    __asm__ __volatile__("outw %1, %0"
                         :
                         : "dN"(port), "a"(value)
                         : "memory");
}

inline void outportl(uint16_t port, uint32_t value)
{
    __asm__ __volatile__("outl %%eax, %%dx"
                         :
                         : "dN"(port), "a"(value)
                         : "memory");
}

void inportsm(uint16_t port, uint8_t *value, unsigned long size)
{
    __asm__ __volatile__("rep insw"
                         : "+D"(value), "+c"(size)
                         : "dN"(port)
                         : "memory");
}

void outportsm(uint16_t port, uint8_t *value, uint16_t size)
{
    __asm__ __volatile__("rep outsw"
                         : "+S"(value), "+c"(size)
                         : "dN"(port)
                         : "memory");
}
